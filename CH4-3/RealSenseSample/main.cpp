#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include "pxcsensemanager.h"

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

        // IRストリームを有効にする
        pxcStatus sts = senseManager->EnableStream(
            PXCCapture::StreamType::STREAM_TYPE_IR,
            IR_WIDTH, IR_HEIGHT, IR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "IRストリームの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );
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

    void updateFrame()
    {
        // フレームを取得する
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // フレームデータを取得する
        const PXCCapture::Sample *sample = senseManager->QuerySample();
        if ( sample ) {
            // 各データを表示する
            updateIrImage( sample->ir );
        }

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    // IR画像を更新する
    void updateIrImage( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = depthFrame->QueryInfo();

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess( 
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_Y8, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("IR画像の取得に失敗");
        }

        // データをコピーする
        irImage = cv::Mat( info.height, info.width, CV_8U );
        memcpy( irImage.data, data.planes[0], data.pitches[0] * info.height );

        // データを解放する
        depthFrame->ReleaseAccess( &data );
    }

    // 画像を表示する
    bool showImage()
    {
        // 表示する
        cv::imshow( "IR Image", irImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat irImage;
    PXCSenseManager *senseManager = 0;

    const int IR_WIDTH = 640;
    const int IR_HEIGHT = 480;
    const int IR_FPS = 30.0f;
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
