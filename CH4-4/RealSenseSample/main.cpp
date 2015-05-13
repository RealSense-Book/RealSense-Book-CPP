#include "pxcsensemanager.h"

#include <iostream>

class RealSenseApp
{
public:

    ~RealSenseApp()
    {
    }

    void initilize()
    {
        // SenseManagerを生成する
        senseManager = PXCSenseManager::CreateInstance();
        if ( senseManager == 0 ) {
            throw std::runtime_error( "SenseManagerの生成に失敗しました" );
        }

        // パイプラインを初期化する
        auto sts = senseManager->Init();
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "パイプラインの初期化に失敗しました" );
        }

        // 使用可能なデバイスを列挙する
        enumDevice();
    }

    void run()
    {
    }

private:

    void enumDevice()
    {
        // セッションを取得する
        auto session = senseManager->QuerySession();
        if ( session == 0 ) {
            throw std::runtime_error( "セッションの取得に失敗しました" );
        }

        // 取得するグループを設定する
        PXCSession::ImplDesc mdesc = {};
        mdesc.group = PXCSession::IMPL_GROUP_SENSOR;
        mdesc.subgroup = PXCSession::IMPL_SUBGROUP_VIDEO_CAPTURE;

        for ( int i = 0;; ++i ) {
            // センサーグループを取得する
            PXCSession::ImplDesc desc1;
            auto sts = session->QueryImpl( &mdesc, i, &desc1 );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                break;
            }

            // センサーグループ名を表示する
            std::wcout << desc1.friendlyName << std::endl;

            // キャプチャーオブジェクトを作成する
            PXCCapture* capture = 0;
            sts = session->CreateImpl<PXCCapture>( &desc1, &capture );
            if ( sts < PXC_STATUS_NO_ERROR ) {
                continue;
            }

            // デバイスを列挙する
            enumDevice( capture );

            // キャプチャーオブジェクトを解放する
            capture->Release();
        }
    }

    void enumDevice( PXCCapture* capture )
    {
        for ( int i = 0;; ++i ) {
            // デバイス情報を取得する
            PXCCapture::DeviceInfo dinfo;
            auto sts = capture->QueryDeviceInfo( i, &dinfo );
            if ( sts < PXC_STATUS_NO_ERROR ){
                break;
            }

            // デバイス名を表示する
            std::wcout << "\t" << dinfo.name << std::endl;

            // デバイスを取得する
            auto device = capture->CreateDevice( i );

            for ( int s = 0; s < PXCCapture::STREAM_LIMIT; ++s ) {
                // ストリーム種別を取得する
                PXCCapture::StreamType type = PXCCapture::StreamTypeFromIndex( s );
                if ( (dinfo.streams & type) == 0 ){
                    continue;
                }

                // ストリーム名を取得する
                const pxcCHAR *name = PXCCapture::StreamTypeToString( type );
                std::wcout << "\t\t" << name << std::endl;

                // ストリームのフォーマットを取得する
                int nprofiles = device->QueryStreamProfileSetNum( type );
                for ( int p = 0; p<nprofiles; ++p ) {
                    PXCCapture::Device::StreamProfileSet profiles = {};
                    sts = device->QueryStreamProfileSet( type, p, &profiles );
                    if ( sts < PXC_STATUS_NO_ERROR ) {
                        break;
                    }

                    // ストリームのフォーマットを表示する
                    std::wcout << "\t\t\t" << Profile2String( &profiles[type] ).c_str()
                               << std::endl;
                }
            }
        }

        std::wcout << std::endl;
    }

    // raw_streams サンプルより
    static std::wstring Profile2String( PXCCapture::Device::StreamProfile *pinfo ) {
        pxcCHAR line[256] = L"";
        if ( pinfo->frameRate.min && pinfo->frameRate.max &&
             pinfo->frameRate.min != pinfo->frameRate.max ) {
            swprintf_s<sizeof( line ) / sizeof( pxcCHAR )>(
                line, L"%s %dx%dx%d-%d",
                PXCImage::PixelFormatToString( pinfo->imageInfo.format ),
                pinfo->imageInfo.width, pinfo->imageInfo.height,
                (int)pinfo->frameRate.min, (int)pinfo->frameRate.max );
        }
        else {
            pxcF32 frameRate = pinfo->frameRate.min ?
                pinfo->frameRate.min : pinfo->frameRate.max;

            swprintf_s<sizeof( line ) / sizeof( pxcCHAR )>(
                line, L"%s %dx%dx%d",
                PXCImage::PixelFormatToString( pinfo->imageInfo.format ),
                pinfo->imageInfo.width, pinfo->imageInfo.height,
                (int)frameRate );
        }
        return std::wstring( line );
    }


private:

    PXCSenseManager *senseManager = 0;
};

void main()
{
    try{
        // コンソールに日本語を表示させる
        setlocale( LC_ALL, "Japanese" );

        RealSenseApp app;
        app.initilize();
        app.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
