// 手指の位置をDepth画像に合わせる
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

        // 手の検出の設定
        PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
        config->SetTrackingMode( PXCHandData::TRACKING_MODE_EXTREMITIES );
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

        // 画像を初期化
        handImage = cv::Mat::zeros( DEPTH_HEIGHT, DEPTH_WIDTH, CV_8UC4 );

        // フレームデータを取得する
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // 各データを表示する
            updateDepthImage( sample->depth );
        }

        // 手の更新
        updateHandFrame();

        // フレームを解放する
        senseManager->ReleaseFrame();
    }


    // Depth画像を更新する
    void updateDepthImage( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depth画像の取得に失敗" );
        }

        // データをコピーする
        PXCImage::ImageInfo info = depthFrame->QueryInfo();
        memcpy( handImage.data, data.planes[0], data.pitches[0] * info.height );

        // データを解放する
        depthFrame->ReleaseAccess( &data );
    }

    void updateHandFrame()
    {
        // 手のデータを更新する
        handData->Update();

        // 点に色を付ける
        const cv::Scalar colors[] = {
            cv::Scalar( 255, 0, 0 ),
            cv::Scalar( 0, 255, 0 ),
            cv::Scalar( 0, 0, 255 ),
            cv::Scalar( 255, 255, 0 ),
            cv::Scalar( 255, 0, 255 ),
            cv::Scalar( 0, 255, 255 ),
        };

        auto numOfHands = handData->QueryNumberOfHands();
        for ( int i = 0; i < numOfHands; i++ ) {
            // 手を取得する
            PXCHandData::IHand* hand;
            auto sts = handData->QueryHandData(
                PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // 位置を列挙する
            for ( int j = 0; j < PXCHandData::NUMBER_OF_EXTREMITIES; j++ ) {
                PXCHandData::ExtremityData extremityData;
                sts = hand->QueryExtremityPoint(
                    (PXCHandData::ExtremityType)j, extremityData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( extremityData.pointImage.x, extremityData.pointImage.y ),
                    10, colors[j], -1 );
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
