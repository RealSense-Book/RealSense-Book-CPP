#include "pxcsensemanager.h"
#include "PXCSpeechSynthesis.h"

#include <opencv2\opencv.hpp>

#include "voice_out.h"

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
        if ( senseManager != nullptr ){
            senseManager->Release();
            senseManager = nullptr;
        }

        if ( synthesis != nullptr ){
            synthesis->Release();
            synthesis = nullptr;
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
        auto sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_COLOR );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "カラーストリームの有効化に失敗しました" );
        }

        // パイプラインを初期化する
        sts = senseManager->Init();
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // 音声認識を初期化する
        initializeSpeechSynthesis();
    }

    void run()
    {
        // メインループ
        while ( 1 ) {
            std::cout << "出力する語を入力してください。qで終了します。" << std::endl;

            // 出力する語を1行取り出す
            std::wstring sentence;
            std::getline( std::wcin, sentence );
            if ( sentence == L"q" ){
                break;
            }

            // 音声合成する
            speechSynthesis( sentence );
        }
    }

private:

    void initializeSpeechSynthesis()
    {
        auto session = senseManager->QuerySession();

        // 音声合成エンジンを列挙する
        std::cout << "音声合成エンジン" << std::endl;

        PXCSession::ImplDesc in = {};
        PXCSession::ImplDesc out = {};
        in.cuids[0] = PXCSpeechSynthesis::CUID;

        for ( int i = 0;; ++i ) {
            // 音声合成エンジンを取得する
            auto sts = session->QueryImpl( &in, i, &out );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // 音声合成エンジン名称を表示する
            std::wcout << "\t" << out.friendlyName << std::endl;
        }


        // 最初の音声合成エンジンを使う
        auto sts = session->QueryImpl( &in, 0, &out );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声合成エンジンの取得に失敗しました" );
        }

        // 音声合成エンジンオブジェクトを作成する
        sts = session->CreateImpl<PXCSpeechSynthesis>( &out, &synthesis );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声合成エンジンオブジェクトの作成に失敗しました" );
        }

        // 対応言語を列挙する
        for ( int j = 0;; ++j ) {
            // 音声合成エンジンが持っているプロファイルを取得する
            PXCSpeechSynthesis::ProfileInfo pinfo;
            sts = synthesis->QueryProfile( j, &pinfo );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // 対応言語を表示する
            std::wcout << "\t\t" << LanguageToString( pinfo.language ) << std::endl;

            // 英語のエンジンを使う(日本語対応時には日本語に変更する)
            if ( pinfo.language == PXCSpeechSynthesis::LANGUAGE_JP_JAPANESE ){
                profile = pinfo;
            }
        }

        if ( profile.language == 0 ){
            throw std::runtime_error( "選択した音声合成エンジンが見つかりませんでした" );
        }

        // 音声合成時のパラメーターを設定する
        profile.volume = 80;
        profile.pitch = 100;
        profile.rate = 100;

        // 使用する言語を設定する
        sts = synthesis->SetProfile( &profile );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "音声合成エンジンオブジェクトの設定に失敗しました" );
        }
    }

    void speechSynthesis( const std::wstring& sentence )
    {
        auto sts = synthesis->BuildSentence( 1, (pxcCHAR*)sentence.c_str() );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "センテンスのビルドに失敗しました" );
        }

        // 音声合成した結果を出力する
        VoiceOut vo( &profile );
        int bufferNum = synthesis->QueryBufferNum( 1 );
        for ( int i = 0; i < bufferNum; ++i ) {
            auto sample = synthesis->QueryBuffer( 1, i );
            vo.RenderAudio( sample );
        }
    }

    pxcCHAR *LanguageToString( PXCSpeechSynthesis::LanguageType language ) {
        switch ( language ) {
        case PXCSpeechSynthesis::LANGUAGE_US_ENGLISH:		return L"US English";
        case PXCSpeechSynthesis::LANGUAGE_GB_ENGLISH:		return L"British English";
        case PXCSpeechSynthesis::LANGUAGE_DE_GERMAN:		return L"Deutsch";
        case PXCSpeechSynthesis::LANGUAGE_IT_ITALIAN:		return L"Italiano";
        case PXCSpeechSynthesis::LANGUAGE_BR_PORTUGUESE:	return L"Português";
        case PXCSpeechSynthesis::LANGUAGE_CN_CHINESE:		return L"中文";
        case PXCSpeechSynthesis::LANGUAGE_FR_FRENCH:		return L"Français";
        case PXCSpeechSynthesis::LANGUAGE_JP_JAPANESE:	    return L"日本語";
        case PXCSpeechSynthesis::LANGUAGE_US_SPANISH:		return L"US Español";
        case PXCSpeechSynthesis::LANGUAGE_LA_SPANISH:		return L"LA Español";
        }
        return 0;
    }

private:

    cv::Mat colorImage;
    PXCSenseManager *senseManager = nullptr;

    PXCSpeechSynthesis *synthesis = nullptr;
    PXCSpeechSynthesis::ProfileInfo profile;
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
