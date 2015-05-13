// 手を検出する
#include "pxcsensemanager.h"
#include "pxchandconfiguration.h"

#include <opencv2\opencv.hpp>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( handAnalyzer == nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( handData == nullptr ){
            handData->Release();
            handData = nullptr;
        }
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == nullptr ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // Depthストリームを有効にする
        auto sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depthストリームの有効化に失敗しました" );
        }

        // 手の検出を有効にする
        sts = senseManager->EnableHand();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "手の検出の有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
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
        PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
        PXCCapture::DeviceInfo dinfo;
        device->QueryDeviceInfo( &dinfo );
        if ( dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM ) {
            device->SetDepthConfidenceThreshold( 1 );
            //device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
            device->SetIVCAMFilterOption( 6 );
        }

        // Hand Module Configuration
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        //config->EnableNormalizedJoints( true );
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

        // 手のデータを更新する
        updateHandFrame();

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    void updateHandFrame()
    {
        handData->Update();

        // 画像を初期化
        handImage1 = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC1 );
        handImage2 = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC1 );

        // 検出した手の数を取得する
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
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // マスク画像を取得する
            PXCImage::ImageData data;
            sts = image->AcquireAccess( 
                PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts < PXC_STATUS_NO_ERROR ){
                continue;
            }

            // マスク画像のサイズはDepthに依存
            // 手は2つまで
            PXCImage::ImageInfo info = image->QueryInfo();
            auto& handImage = (i == 0) ? handImage1 : handImage2;
            memcpy( handImage.data, data.planes[0], info.height * info.width );

            // データを表示させる(時間がかかります)
            //for ( int i = 0; i < info.height * info.width; i++ ){
            //    std::cout << (int)data.planes[0][i] << " ";
            //}

            image->ReleaseAccess( &data );
        }
    }

    // 画像を表示する
    bool showImage()
    {
        // 表示する
        cv::imshow( "Hand Image 1", handImage1 );
        cv::imshow( "Hand Image 2", handImage2 );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    PXCSenseManager* senseManager = nullptr;

    cv::Mat handImage1;
    cv::Mat handImage2;

    PXCHandModule* handAnalyzer = nullptr;
    PXCHandData* handData = nullptr;

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
