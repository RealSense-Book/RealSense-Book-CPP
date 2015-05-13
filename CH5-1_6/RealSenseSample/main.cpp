// 手指の位置をカラー画像に合わせる
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"
#include "PXCProjection.h"

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != 0 ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( projection != 0 ){
            projection->Release();
            projection = nullptr;
        }

        if ( handData != 0 ){
            handData->Release();
            handData = nullptr;
        }

        if ( handAnalyzer != 0 ){
            handAnalyzer->Release();
            handAnalyzer = nullptr;
        }
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // カラーストリームを有効にする
        auto sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_COLOR,
            COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "カラーストリームの有効化に失敗しました" );
        }

        // Depthストリームを有効にする
        sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
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

        // デバイスを取得する
        auto device = senseManager->QueryCaptureManager()->QueryDevice();

        // ミラー表示にする
        device->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // 座標変換オブジェクトを作成
        projection = device->CreateProjection();

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
        pxcStatus sts = senseManager->AcquireFrame( true );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // 画像を初期化
        handImage = cv::Mat::zeros( COLOR_HEIGHT, COLOR_WIDTH, CV_8UC3 );

        // フレームデータを取得する
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // 各データを表示する
            updateColorImage( sample->color );
        }

        // 手の更新
        updateHandFrame();

        // フレームを解放する
        senseManager->ReleaseFrame();
    }


    // カラー画像を更新する
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "カラー画像の取得に失敗" );
        }

        // データをコピーする
        PXCImage::ImageInfo info = colorFrame->QueryInfo();
        memcpy( handImage.data, data.planes[0], info.height * info.width * 3 );

        // データを解放する
        colorFrame->ReleaseAccess( &data );
    }

    void updateHandFrame()
    {
        // 手のデータを更新する
        handData->Update();

        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // 手を取得する
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // 指の関節を列挙する
            for ( int j = 0; j < PXCHandData::NUMBER_OF_JOINTS; j++ ) {
                //  指のデータを取得する
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                // Depth座標系をカラー座標系に変換する
                PXCPointF32 colorPoint = { 0 };
                auto depthPoint = jointData.positionImage;
                depthPoint.z = jointData.positionWorld.z * 1000;
                projection->MapDepthToColor( 1, &depthPoint, &colorPoint );

                // 指の座標を表示する
                cv::circle( handImage,
                    cv::Point( colorPoint.x, colorPoint.y ),
                    5, cv::Scalar( 255, 255, 0 ), -1 );
            }
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

    PXCProjection *projection = 0;

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30;

    const int COLOR_WIDTH = 1280;
    const int COLOR_HEIGHT = 720;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1920;
    //const int COLOR_HEIGHT = 1080;
    //const int COLOR_FPS = 30;
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
