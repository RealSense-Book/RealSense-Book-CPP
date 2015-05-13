// 指のデータを表示する
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != 0 ){
            senseManager->Release();
        }
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // Depthストリームを有効にする
        pxcStatus sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depthストリームの有効化に失敗しました" );
        }

        // 手の検出を有効にする
        sts = senseManager->EnableHand();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "手の検出の有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // 手の検出の初期化
        initializeHandTracking();
    }

    void run()
    {
        // メインループ
        while ( 1 ) {
            // フレームデータを更新する
            updateFrame();

            // 表示する
            auto ret = showImage();
            if ( !ret ){
                break;
            }
        }
    }

private:

    void initializeHandTracking()
    {
        // 手の検出器を作成する
        handAnalyzer = senseManager->QueryHand();
        if ( handAnalyzer == 0 ) {
            throw std::runtime_error( "手の検出器の取得に失敗しました" );
        }

        // 手のデータを取得する
        handData = handAnalyzer->CreateOutput();
        if ( handData == 0 ) {
            throw std::runtime_error( "手の検出器の作成に失敗しました" );
        }

        // RealSense カメラであれば、プロパティを設定する
        PXCCapture::DeviceInfo dinfo;
        senseManager->QueryCaptureManager()->QueryDevice()->QueryDeviceInfo( &dinfo );
        if ( dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM ) {
            PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
            device->SetDepthConfidenceThreshold( 1 );
            //device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
            device->SetIVCAMFilterOption( 6 );
        }

        // Hand Module Configuration
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        //config->EnableNormalizedJoints( showNormalizedSkeleton );
        //config->SetTrackingMode( PXCHandData::TRACKING_MODE_EXTREMITIES );
        //config->EnableAllAlerts();
        config->EnableSegmentationImage( true );

        config->ApplyChanges();
        config->Update();
    }

    void updateFrame()
    {
        // フレームを取得する
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // 手の更新
        updateHandFrame();

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    void updateHandFrame()
    {
        // 手のデータを更新する
        handData->Update();

        // 画像を初期化
        handImage = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC3 );

        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // 手を取得する
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // 手のマスク画像を取得する
            PXCImage* image = 0;
            sts = hand->QuerySegmentationImage( image );
            if ( sts != PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // 手の画像データを取得する
            PXCImage::ImageData data;
            sts = image->AcquireAccess( PXCImage::ACCESS_READ,
                PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts != PXC_STATUS_NO_ERROR ){
                continue;
            }

            // 手の左右を取得する
            auto side = hand->QueryBodySide();

            // 手の開閉度(0-100)を取得する
            auto openness = hand->QueryOpenness();


            // マスク画像のサイズはDepthに依存
            // 手は2つまで検出する
            PXCImage::ImageInfo info = image->QueryInfo();
            for ( int j = 0; j < info.height * info.width; ++j ){
                if ( data.planes[0][j] != 0 ){
                    auto index = j * 3;

                    // 手の左右および手の開閉度で色合いを決める
                    // 左手=1：0-127の範囲
                    // 右手=2：0-254の範囲
                    auto value = (uchar)((side * 127) * (openness / 100.0f));

                    handImage.data[index + 0] = value;
                    handImage.data[index + 1] = value;
                    handImage.data[index + 2] = value;
                }
            }

            // 指の関節を列挙する
            for ( int j = 0; j < PXCHandData::NUMBER_OF_JOINTS; j++ ) {
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( jointData.positionImage.x, jointData.positionImage.y ),
                    5, cv::Scalar( 128, 128, 0 ) );
            }

            // 手の画像データを解放する
            image->ReleaseAccess( &data );

            // 手の中心を表示する
            auto center = hand->QueryMassCenterImage();
            cv::circle( handImage, cv::Point( center.x, center.y ), 5,
                cv::Scalar( 255, 0, 0 ), -1 );

            // 手の範囲を表示する
            auto boundingbox = hand->QueryBoundingBoxImage();
            cv::rectangle( handImage,
                cv::Rect( boundingbox.x, boundingbox.y, boundingbox.w, boundingbox.h ),
                cv::Scalar( 0, 0, 255 ), 2 );
        }
    }

    // 画像を表示する
    bool showImage()
    {
        // 表示する
        cv::imshow( "Hand Image", handImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    PXCSenseManager* senseManager = 0;

    cv::Mat handImage;

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30;
};

void main()
{
    try{
        RealSenseApp app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
