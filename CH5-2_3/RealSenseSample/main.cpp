// 輪郭モードで動作させる
#include "pxcsensemanager.h"
#include "pxcblobmodule.h"

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

        if ( blobData != nullptr ){
            blobData->Release();
            blobData = nullptr;
        }

        if ( blobModule != nullptr ){
            blobModule->Release();
            blobModule = nullptr;
        }
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // Blobを有効にする
        pxcStatus sts = senseManager->EnableBlob();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "Blobの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // Blobを初期化する
        initializeBlob();
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

    // Blobを初期化する
    void initializeBlob()
    {
        // Blobを取得する
        blobModule = senseManager->QueryBlob();
        blobData = blobModule->CreateOutput();

        auto blobConfig = blobModule->CreateActiveConfiguration();
        blobConfig->SetSegmentationSmoothing( 1.0f );
        blobConfig->SetContourSmoothing( 1.0f );
        blobConfig->SetMaxBlobs( 4 );
        blobConfig->SetMaxDistance( 500.0f );
        blobConfig->EnableContourExtraction( true );
        blobConfig->EnableSegmentationImage( true );
        blobConfig->ApplyChanges();

        // 輪郭の点の配列を初期化
        points.resize( 4000 );
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
            updateBlobImage( sample->depth );
        }

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    // Blobを更新する
    void updateBlobImage( PXCImage* depthFrame )
    {
        if ( depthFrame == nullptr ){
            return;
        }

        // Blobを更新する
        auto sts = blobData->Update();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // 表示用画像を初期化する
        PXCImage::ImageInfo depthInfo = depthFrame->QueryInfo();
        contourImage = cv::Mat::zeros( depthInfo.height, depthInfo.width, CV_8U );

        auto session = senseManager->QuerySession();
        depthInfo.format = PXCImage::PIXEL_FORMAT_Y8;
        PXCImage* blobImage = session->CreateImage( &depthInfo );

        // Blobを取得する
        int numOfBlobs = blobData->QueryNumberOfBlobs();
        for ( int i = 0; i < numOfBlobs; ++i ) {
            // Blobデータを近くから順に取得する
            PXCBlobData::IBlob* blob;
            sts = blobData->QueryBlobByAccessOrder( i, 
                PXCBlobData::AccessOrderType::ACCESS_ORDER_NEAR_TO_FAR, blob );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // Blob画像を取得する
            sts = blob->QuerySegmentationImage( blobImage );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // Blob画像を読み込む
            PXCImage::ImageData data;
            pxcStatus sts = blobImage->AcquireAccess( PXCImage::Access::ACCESS_READ,
                PXCImage::PIXEL_FORMAT_Y8, &data );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // データをコピーする
            for ( int j = 0; j < depthInfo.height * depthInfo.width; ++j ){
                if ( data.planes[0][j] != 0 ){
                    // インデックスによって、色味を変える
                    contourImage.data[j] = (i + 1) * 64;
                }
            }

            // Blob画像を解放する
            blobImage->ReleaseAccess( &data );

            // Blobの輪郭を表示する
            updateContoursImage( blob, i );
        }

        // 解放するとエラーになる
        //blobImage->Release();
    }

    // Blobの輪郭を表示する
    void updateContoursImage( PXCBlobData::IBlob* blob, int index )
    {
        // 輪郭を表示する
        auto numOfContours = blob->QueryNumberOfContours();
        for ( int i = 0; i < numOfContours; ++i ) {
            // 輪郭の点の数を取得する
            pxcI32 size = blob->QueryContourSize( i );
            if ( size <= 0 ) {
                continue;
            }

            // ポイント配列の確認
            if ( points.size() < size ){
                points.reserve( size );
            }

            // 輪郭の点を取得する
            auto sts = blob->QueryContourPoints( i, points.size(), &points[0] );
            if ( sts < PXC_STATUS_NO_ERROR ){
                continue;
            }

            // 輪郭の点を描画する
            drawContour( &points[0], size, index );
        }
    }

    // 輪郭の点を描画する
    void drawContour( PXCPointI32* points, pxcI32 size, int index )
    {
        // 点と点を線で結ぶ
        for ( int i = 0; i < (size - 1); ++i ){
            const auto& pt1 = points[i];
            const auto& pt2 = points[i + 1];
            cv::line( contourImage, cv::Point( pt1.x, pt1.y ), cv::Point( pt2.x, pt2.y ),
                cv::Scalar( ((index + 1) * 127) ), 5 );
        }

        // 最後の点と最初の点を線で結ぶ
        const auto& pt1 = points[size - 1];
        const auto& pt2 = points[0];
        cv::line( contourImage, cv::Point( pt1.x, pt1.y ), cv::Point( pt2.x, pt2.y ),
            cv::Scalar( ((index + 1) * 127) ), 5 );
    }

    // 画像を表示する
    bool showImage()
    {
        if ( contourImage.cols != 0 ){
            cv::imshow( "Contour Image", contourImage );
        }

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }

        return true;
    }

private:

    cv::Mat contourImage;

    PXCSenseManager* senseManager = nullptr;

    PXCBlobModule* blobModule = nullptr;
    PXCBlobData* blobData = nullptr;
    std::vector<PXCPointI32> points;
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
