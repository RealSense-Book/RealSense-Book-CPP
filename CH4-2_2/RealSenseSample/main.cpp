// Depthの距離データを利用する
#include "pxcsensemanager.h"

#include <opencv2\opencv.hpp>

class RealSenseAsenseManager
{
public:

    ~RealSenseAsenseManager()
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
        pxcStatus sts = senseManager->EnableStream(
            PXCCapture::StreamType::STREAM_TYPE_DEPTH,
            DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depthストリームの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // Depth距離表示の初期化
        initializeDepth();
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

    void initializeDepth()
    {
        // バッファを作成する
        depthBuffer.resize( DEPTH_WIDTH * DEPTH_HEIGHT );

        // 中心点で初期化する
        point.x = DEPTH_WIDTH / 2;
        point.y = DEPTH_HEIGHT / 2;

        // マウス入力を受け取るようにする
        cv::namedWindow( windowName, CV_WINDOW_AUTOSIZE );
        cv::setMouseCallback( windowName, &RealSenseAsenseManager::mouse_callback, this );
    }

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
            updateDepthImage( sample->depth );
            updateDepthData( sample->depth );
            showSelectedDepth();
        }

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
        depthImage = cv::Mat( info.height, info.width, CV_8UC4 );
        memcpy( depthImage.data, data.planes[0], data.pitches[0] * info.height );

        // データを解放する
        depthFrame->ReleaseAccess( &data );
    }

    // Depth(距離データ)を更新する
    void updateDepthData( PXCImage* depthFrame )
    {
        if ( depthFrame == 0 ){
            return;
        }

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = depthFrame->AcquireAccess(
            PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_DEPTH, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Depthデータの取得に失敗" );
        }

        // データをコピーする
        PXCImage::ImageInfo info = depthFrame->QueryInfo();
        memcpy( &depthBuffer[0], data.planes[0], data.pitches[0] * info.height );

        // データを解放する
        depthFrame->ReleaseAccess( &data );
    }

    // 選択位置の距離を表示する
    void showSelectedDepth()
    {
        cv::circle( depthImage, point, 10, cv::Scalar( 0, 0, 255 ), 3 );

        int index = (point.y * DEPTH_WIDTH) + point.x;
        auto depth = depthBuffer[index];

        {
            std::stringstream ss;
            ss << depth << "mm";
            cv::putText( depthImage, ss.str(), point,
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 128, 255, 128 ), 2, CV_AA );
        }
    }
    
    // 画像を表示する
    bool showImage()
    {
        if ( depthImage.rows == 0 || (depthImage.cols == 0) ) {
            return true;
        }

        // 表示する
        cv::imshow( windowName, depthImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

    static void mouse_callback( int event, int x, int y, int flags, void* param )
    {
        RealSenseAsenseManager* pThis = (RealSenseAsenseManager*)param;
        pThis->mouse_callback( event, x, y, flags );
    }

    void mouse_callback( int event, int x, int y, int flags )
    {
        if ( event == cv::EVENT_LBUTTONDOWN ){
            point.x = x;
            point.y = y;
        }
    }

private:

    cv::Mat depthImage;
    PXCSenseManager *senseManager = 0;

    std::vector<unsigned short> depthBuffer;
    cv::Point point;

    const std::string windowName = "Depth Image";

    const int DEPTH_WIDTH = 640;
    const int DEPTH_HEIGHT = 480;
    const int DEPTH_FPS = 30.0f;
};

void main()
{
    try{
        RealSenseAsenseManager app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
