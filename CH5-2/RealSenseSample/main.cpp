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
        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( handConfig != nullptr ){
            handConfig->Release();
            handConfig = nullptr;
        }

        if ( handData != nullptr ){
            handData->Release();
            handData = nullptr;
        }

        if ( handAnalyzer != nullptr ){
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

        // 手の設定
        handConfig = handAnalyzer->CreateActiveConfiguration();

        // 登録されているジェスチャーを列挙する
        auto num = handConfig->QueryGesturesTotalNumber();
        for ( int i = 0; i < num; i++ ){
            pxcCHAR gestureName[PXCHandData::MAX_NAME_SIZE];
            auto sts = handConfig->QueryGestureNameByIndex( i,
                PXCHandData::MAX_NAME_SIZE, gestureName );
            if ( sts == PXC_STATUS_NO_ERROR ){
                std::wcout << std::hex << i << " " <<  gestureName << std::endl;
            }
        }

        handConfig->ApplyChanges();
        handConfig->Update();
    }

    void updateFrame()
    {
        // フレームを取得する
        pxcStatus sts = senseManager->AcquireFrame( true );
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

        // 認識した手の数を取得する
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
                PXCHandData::JointData jointData;
                sts = hand->QueryTrackedJoint( (PXCHandData::JointType)j, jointData );
                if ( sts != PXC_STATUS_NO_ERROR ) {
                    continue;
                }

                cv::circle( handImage,
                    cv::Point( jointData.positionImage.x, jointData.positionImage.y ),
                    5, cv::Scalar( 128, 128, 0 ) );
            }
        }

        // 認識したジェスチャーの数を取得する
        auto numOfGestures = handData->QueryFiredGesturesNumber();
        for ( int i = 0; i < numOfGestures; i++ ) {
            // 認識したジェスチャーを取得する
            PXCHandData::GestureData gesture;
            auto sts = handData->QueryFiredGestureData( i, gesture );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // ジェスチャーをした手を取得する
            PXCHandData::IHand* hand;
            sts = handData->QueryHandDataById( gesture.handId, hand );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // どちらの手でジェスチャーしたのか
            auto side = hand->QueryBodySide();
            if ( side == PXCHandData::BodySideType::BODY_SIDE_LEFT ){
                ++leftGestureCount;
            }
            else {
                ++rightGestureCount;
            }
        }

        // ジェスチャーの検出数を表示する
        {
            std::stringstream ss;
            ss << "Left gesture  : " << leftGestureCount;
            cv::putText( handImage, ss.str(), cv::Point( 10, 40 ),
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
        }

        {
            std::stringstream ss;
            ss << "Right gesture : " << rightGestureCount;
            cv::putText( handImage, ss.str(), cv::Point( 10, 80 ),
                cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
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
        // 0-9キーでジェスチャーの選択(0-9)
        else if ( ('0' <= c) && (c <= '9') ) {
            // キーをインデックスに変換する(0-9)
            int index = c - '0';
            ChangeGesture( index );
        }
        // a-dキーでジェスチャーの選択(10-13)
        else if ( ('a' <= c) && (c <= 'd') ) {
            // キーをインデックスに変換する(10-13)
            int index = c - 'a' + 10;
            ChangeGesture( index );
        }

        return true;
    }

    void ChangeGesture( int index )
    {
        // インデックスのジェスチャー名を取得する
        pxcCHAR gestureName[PXCHandData::MAX_NAME_SIZE];
        auto sts = handConfig->QueryGestureNameByIndex( index,
            PXCHandData::MAX_NAME_SIZE, gestureName );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // 一度すべてのジェスチャーを切り、選択されたジェスチャーを有効にする
        handConfig->DisableAllGestures();
        handConfig->EnableGesture( gestureName, true );

        handConfig->ApplyChanges();

        // ジェスチャー検出数の初期化
        rightGestureCount = leftGestureCount = 0;

        std::wcout << gestureName << " selected" << std::endl;
    }

private:

    cv::Mat handImage;

    PXCSenseManager* senseManager = 0;

    PXCHandModule* handAnalyzer = 0;
    PXCHandData* handData = 0;

    PXCHandConfiguration* handConfig = 0;
    int rightGestureCount = 0;
    int leftGestureCount = 0;

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

