#include "pxcsensemanager.h"
#include "PXCSpeechRecognition.h"

#include <opencv2\opencv.hpp>

class RealSenseApp : public PXCSpeechRecognition::Handler
{
public:

    ~RealSenseApp()
    {

        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( audioSource != nullptr ){
            audioSource->Release();
            audioSource = nullptr;
        }

        if ( recognition != nullptr ){
            recognition->Release();
            recognition = nullptr;
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
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "カラーストリームの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // ミラー表示にする
        senseManager->QueryCaptureManager()->QueryDevice()->SetMirrorMode(
            PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL );

        // 音声認識を初期化する
        initializeVoiceRecognition();
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

    void initializeVoiceRecognition()
    {
        auto session = senseManager->QuerySession();

        // 音声入力デバイスを作成する
        audioSource = session->CreateAudioSource();
        if ( audioSource == 0 ){
            throw std::runtime_error( "音声入力デバイスの作成に失敗しました" );
        }

        // 音声入力デバイスを列挙する
        std::cout << "音声入力デバイス" << std::endl;

        PXCAudioSource::DeviceInfo device = {};

        // 使用可能なデバイスをスキャンする
        audioSource->ScanDevices();

        auto deviceNum = audioSource->QueryDeviceNum();
        for ( int i = 0; i < deviceNum; ++i ) {
            PXCAudioSource::DeviceInfo dinfo = {};
            auto sts = audioSource->QueryDeviceInfo( i, &dinfo );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                throw std::runtime_error( "デバイス情報の取得に失敗しました" );
            }

            // 音声入力デバイス名を表示する
            std::wcout << "\t" << dinfo.name << std::endl;

            // 最初のデバイスを使う
            if ( i == 0 ){
                device = dinfo;
            }
        }

        // 音声入力デバイスを設定する
        auto sts = audioSource->SetDevice( &device );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声入力デバイスの設定に失敗しました" );
        }


        // 音声認識エンジンを列挙する
        std::cout << "音声認識エンジン" << std::endl;

        PXCSession::ImplDesc in = {};
        PXCSession::ImplDesc out = {};
        PXCSession::ImplDesc desc = {};
        in.cuids[0] = PXCSpeechRecognition::CUID;

        for ( int i = 0; ; ++i ) {
            // 音声認識エンジンを取得する
            auto sts = session->QueryImpl( &in, i, &out );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // 音声認識エンジン名称を表示する
            std::wcout << "\t" << out.friendlyName << std::endl;

            // 最初の音声認識エンジンを使う
            if ( i == 0 ){
                desc = out;
            }
        }

        // 音声認識エンジンオブジェクトを作成する
        sts = session->CreateImpl<PXCSpeechRecognition>( &desc, &recognition );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声認識エンジンオブジェクトの作成に失敗しました" );
        }

        // 対応言語を列挙する
        PXCSpeechRecognition::ProfileInfo profile = {};

        for ( int j = 0;; ++j ) {
            // 音声認識エンジンが持っているプロファイルを取得する
            PXCSpeechRecognition::ProfileInfo pinfo;
            sts = recognition->QueryProfile( j, &pinfo );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // 対応言語を表示する
            std::wcout << "\t\t" << LanguageToString( pinfo.language ) << std::endl;

            // 日本語のエンジンを使う
            if ( pinfo.language == PXCSpeechRecognition::LANGUAGE_JP_JAPANESE ){
                profile = pinfo;
            }
        }

        if ( profile.language == 0 ){
            throw std::runtime_error( "選択した音声認識エンジンが見つかりませんでした" );
        }

        // 使用する言語を設定する
        sts = recognition->SetProfile( &profile );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声認識エンジンオブジェクトの設定に失敗しました" );
        }

        setDictationMode();

        // 音声認識を開始する
        sts = recognition->StartRec( audioSource, this );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声認識の開始に失敗しました" );
        }
    }

    // ディクテーションモードに設定する
    void setDictationMode()
    {
        // ディクテーションモードを設定する
        auto sts = recognition->SetDictation();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "ディクテーションモードの設定に失敗しました" );
        }
    }

    // voice_recognition サンプルより
    pxcCHAR *LanguageToString( PXCSpeechRecognition::LanguageType language ) {
        switch ( language ) {
        case PXCSpeechRecognition::LANGUAGE_US_ENGLISH:		return L"US English";
        case PXCSpeechRecognition::LANGUAGE_GB_ENGLISH:		return L"British English";
        case PXCSpeechRecognition::LANGUAGE_DE_GERMAN:		return L"Deutsch";
        case PXCSpeechRecognition::LANGUAGE_IT_ITALIAN:		return L"Italiano";
        case PXCSpeechRecognition::LANGUAGE_BR_PORTUGUESE:	return L"Português";
        case PXCSpeechRecognition::LANGUAGE_CN_CHINESE:		return L"中文";
        case PXCSpeechRecognition::LANGUAGE_FR_FRENCH:		return L"Français";
        case PXCSpeechRecognition::LANGUAGE_JP_JAPANESE:	return L"日本語";
        case PXCSpeechRecognition::LANGUAGE_US_SPANISH:		return L"US Español";
        case PXCSpeechRecognition::LANGUAGE_LA_SPANISH:		return L"LA Español";
        }
        return 0;
    }

    virtual void PXCAPI OnRecognition( const PXCSpeechRecognition::RecognitionData *data ) {
        std::cout << "Dictation" << std::endl;

        // ディクテーションモードの時はラベルがマイナスの値になる
        if ( data->scores[0].label < 0 ) {
            std::wcout << data->scores[0].sentence << std::endl;
        }
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
            updateColorImage( sample->color );
        }

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
        pxcStatus sts = colorFrame->AcquireAccess( PXCImage::Access::ACCESS_READ, PXCImage::PixelFormat::PIXEL_FORMAT_RGB24, &data );
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

        return true;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = nullptr;

    PXCAudioSource *audioSource = nullptr;
    PXCSpeechRecognition *recognition = nullptr;

    const int COLOR_WIDTH = 640;
    const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;

    //const int COLOR_WIDTH = 1920;
    //const int COLOR_HEIGHT = 1080;
    //const int COLOR_FPS = 30;
};

void main()
{
    try{
        // コンソールに日本語を表示させる
        setlocale( LC_ALL, "Japanese" );

        RealSenseApp asenseManager;
        asenseManager.initilize();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
