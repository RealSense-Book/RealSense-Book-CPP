#include <iostream>
#include <sstream>

#include <windows.h>

#include "pxcsensemanager.h"

#include <opencv2\opencv.hpp>

class RealSenseAsenseManager
{
public:

    ~RealSenseAsenseManager()
    {
        if ( scanner != nullptr ){
            scanner->Release();
            scanner = nullptr;
        }

        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // 3Dスキャンを有効にする
        pxcStatus sts = senseManager->Enable3DScan();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "3Dスキャンの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // 3Dスキャンの初期化
        initialize3dScan();
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

    void initialize3dScan()
    {
        // スキャナーを取得する
        scanner = senseManager->Query3DScan();
        if ( scanner == nullptr ){
            throw std::runtime_error( "スキャナーの取得に失敗しました" );
        }

        // ターゲットオプションの設定
        setTargetingOption(
            PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS );

        // スキャンモードの設定
        setScanMode( PXC3DScan::Mode::TARGETING );

        // モデル作成オプションの表示
        showReconstructionOption();
        showModelFormat();
    }

    void updateFrame()
    {
        // フレームを取得する
        pxcStatus sts = senseManager->AcquireFrame( false );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

        // フレームデータを取得する
        updateColorImage( scanner->AcquirePreviewImage() );

        // フレームを解放する
        senseManager->ReleaseFrame();
    }

    // カラー画像を更新する
    void updateColorImage( PXCImage* colorFrame )
    {
        if ( colorFrame == 0 ){
            return;
        }
            
        PXCImage::ImageInfo info = colorFrame->QueryInfo();

        // データを取得する
        PXCImage::ImageData data;
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ,
            PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error("カラー画像の取得に失敗");
        }

        // データをコピーする
        colorImage = cv::Mat( info.height, info.width, CV_8UC3 );
        memcpy( colorImage.data, data.planes[0], info.height * info.width * 3 );

        // データを解放する
        colorFrame->ReleaseAccess( &data );
    }

    // 画像を表示する
    bool showImage()
    {
        // 表示する
        cv::imshow( "Color Image", colorImage );

        int c = cv::waitKey( 10 );
        if ( (c == 27) || (c == 'q') || (c == 'Q') ){
            // ESC|q|Q for Exit
            return false;
        }
        else if ( c == 't' ) {
            // ターゲットオプションを変更する
            auto option = scanner->QueryTargetingOptions();
            if ( option == PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS ){
                setTargetingOption(
                    PXC3DScan::TargetingOption::OBJECT_ON_PLANAR_SURFACE_DETECTION );
            }
            else{
                setTargetingOption(
                    PXC3DScan::TargetingOption::NO_TARGETING_OPTIONS );
            }
        }
        else if ( c == 's' ) {
            // スキャンモードを変更する
            auto scanMode = scanner->QueryMode();
            if ( scanMode == PXC3DScan::Mode::TARGETING ){
                setScanMode( PXC3DScan::Mode::SCANNING );
            }
            else{
                setScanMode( PXC3DScan::Mode::TARGETING );
            }
        }
        else if ( c == 'o' ){
            // モデル作成オプションを変更する
            changeReconstructionOption();
        }
        else if ( c == 'f' ){
            // モデル作成フォーマットを変更する
            changeModelFormat();
        }
        else if ( c == 'r' ){
            // モデルを作成する
            reconstruct();
        }

        return true;
    }

    // ターゲットオプションを設定する
    void setTargetingOption( PXC3DScan::TargetingOption targetingOption )
    {
        std::cout << "setTargetingOption " << targetingOption << std::endl;
        auto sts = scanner->SetTargetingOptions( targetingOption );
        if ( sts < PXC_STATUS_NO_ERROR ){
            throw std::runtime_error( "ターゲットオプションの設定に失敗しました" );
        }
    }

    // スキャンモードを設定する
    void setScanMode( PXC3DScan::Mode scanMode )
    {
        std::cout << "setScanMode " << scanMode << std::endl;
        auto sts = scanner->SetMode( scanMode );
        if ( sts < PXC_STATUS_NO_ERROR ){
            throw std::runtime_error( "スキャンモードの設定に失敗しました" );
        }
    }

    // モデル作成オプションを変更する
    void changeReconstructionOption()
    {
        if ( reconstructionOption ==
            PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS ){
            reconstructionOption = PXC3DScan::ReconstructionOption::SOLIDIFICATION;
        }
        else {
            reconstructionOption = PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS;
        }

        showReconstructionOption();
    }

    // モデル作成オプションを表示する
    void showReconstructionOption()
    {
        auto option = (reconstructionOption ==
            PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS) ?
                "NO_RECONSTRUCTION_OPTIONS" : "SOLIDIFICATION";

        std::cout << "Reconstruction Option : " << option << std::endl;
    }

    // モデルフォーマットを変更する
    void changeModelFormat()
    {
        if ( fileFormat == PXC3DScan::FileFormat::OBJ ){
            fileFormat = PXC3DScan::FileFormat::STL;
        }
        else if ( fileFormat == PXC3DScan::FileFormat::STL ){
            fileFormat = PXC3DScan::FileFormat::PLY;
        }
        else {
            fileFormat = PXC3DScan::FileFormat::OBJ;
        }

        showModelFormat();
    }

    // モデルフォーマットを表示する
    void showModelFormat()
    {
        std::wcout << L"Model Format          : " <<
            PXC3DScan::FileFormatToString( fileFormat ) << std::endl;
    }


    // モデルを作成する
    void reconstruct()
    {
        // スキャン中以外はモデルを作成しない
        auto scanMode = scanner->QueryMode();
        if ( scanMode != PXC3DScan::Mode::SCANNING ){
            return;
        }

        // ファイル名を作成する
        WCHAR fileTitle[MAX_PATH];
        GetTimeFormatEx( 0, 0, 0, L"hhmmss", fileTitle, _countof( fileTitle ) );

        std::wstringstream ss;
        ss << L"model-" << fileTitle << L"." << PXC3DScan::FileFormatToString( fileFormat );

        std::wcout << L"create " << ss.str() << "...";


        // 3Dモデルを作成する
        scanner->Reconstruct( fileFormat, ss.str().c_str(), reconstructionOption );

        std::cout << "done." << std::endl;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = nullptr;
    PXC3DScan* scanner = nullptr;

    PXC3DScan::ReconstructionOption reconstructionOption =
        PXC3DScan::ReconstructionOption::NO_RECONSTRUCTION_OPTIONS;
    PXC3DScan::FileFormat fileFormat = PXC3DScan::FileFormat::OBJ;
};

void main()
{
    try{
        RealSenseAsenseManager asenseManager;
        asenseManager.initilize();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
