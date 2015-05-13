#include "pxcsensemanager.h"
#include "PXC3DSeg.h"

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

        if ( segmentation != nullptr ){
            segmentation->Release();
            segmentation = nullptr;
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

        // セグメンテーションを有効にする
        sts = senseManager->Enable3DSeg();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "セグメンテーションの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // セグメンテーションオブジェクトを取得する
        segmentation = senseManager->Query3DSeg();
        if ( segmentation == 0 ) {
            throw std::runtime_error( "セグメンテーションの取得に失敗しました" );
        }
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
        auto image = segmentation->AcquireSegmentedImage();
        // 各データを表示する
        updateSegmentationImage( image );

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    // セグメンテーション画像を更新する
    void updateSegmentationImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("カラー画像の取得に失敗");
        }

        // データをコピーする
        colorImage = cv::Mat::zeros( info.height, info.width, CV_8UC4 );

        auto dst = colorImage.data;
        auto src = data.planes[0];

        for ( int i = 0; i < (info.height * info.width); i++ ) {
            auto index = i * BYTE_PER_PIXEL;

            // α値が0でない場合には有効な場所として色をコピーする
            if ( src[index + 3] > 0 ){
                dst[index + 0] = src[index + 0];
                dst[index + 1] = src[index + 1];
                dst[index + 2] = src[index + 2];
            }
            // α値が0の場合は白にする
            else {
                dst[index + 0] = 255;
                dst[index + 1] = 255;
                dst[index + 2] = 255;
            }
        }

        // データを解放する
        colorFrame->ReleaseAccess( &data );
    }

    // 画像を表示する
    bool showImage()
    {
        if ( colorImage.cols == 0 ){
            return true;
        }

        // 表示する
        cv::imshow( "Color Image", colorImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = 0;
    PXC3DSeg* segmentation = 0;

    // ピクセルあたりのバイト数
    const int BYTE_PER_PIXEL = 4;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1280;
    //const int COLOR_HEIGHT = 720;
    //const int COLOR_FPS = 30;
};

void main()
{
    try{
        RealSenseApp asenseManager;
        asenseManager.initilize();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
