#include <sstream>

#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#include "pxcsensemanager.h"
#include "PXCFaceConfiguration.h"

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

        // カラーストリームを有効にする
        pxcStatus sts = senseManager->EnableStream( PXCCapture::StreamType::STREAM_TYPE_COLOR, COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS );
        if ( sts<PXC_STATUS_NO_ERROR ) {
            throw std::runtime_error( "カラーストリームの有効化に失敗しました" );
        }

        
    }

	void initilizeFace(){
		// 顔検出を有効にする
		auto sts = senseManager->EnableFace();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("顔検出の有効化に失敗しました");
		}

		//顔検出器を生成する
		PXCFaceModule* faceModule = senseManager->QueryFace();
		if (faceModule == 0) {
			throw std::runtime_error("顔検出器の作成に失敗しました");
		}

		//顔検出のプロパティを取得
		PXCFaceConfiguration* config = faceModule->CreateActiveConfiguration();
		if (config == 0) {
			throw std::runtime_error("顔検出のプロパティ取得に失敗しました");
		}

		config->SetTrackingMode(PXCFaceConfiguration::TrackingModeType::FACE_MODE_COLOR_PLUS_DEPTH);
		config->ApplyChanges();

		// パイプラインを初期化する
		sts = senseManager->Init();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("パイプラインの初期化に失敗しました");
		}

		// デバイス情報の取得
		auto device = senseManager->QueryCaptureManager()->QueryDevice();
		if (device == 0) {
			throw std::runtime_error("デバイスの取得に失敗しました");
		}


		// ミラー表示にする
		device->SetMirrorMode(PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL);

		PXCCapture::DeviceInfo deviceInfo;
		device->QueryDeviceInfo(&deviceInfo);
		if (deviceInfo.model == PXCCapture::DEVICE_MODEL_IVCAM) {
			device->SetDepthConfidenceThreshold(1);
			device->SetIVCAMFilterOption(6);
			device->SetIVCAMMotionRangeTradeOff(21);
		}

		config->detection.isEnabled = true;
		config->detection.maxTrackedFaces = DETECTION_MAXFACES;
		//config->pose.isEnabled = true;
		//config->landmarks.isEnabled = true;
		//config->QueryExpressions()->Enable();
		//config->QueryExpressions()->EnableAllExpressions();
		//config->QueryRecognition()->Enable();
		//config->QueryExpressions()->properties.maxTrackedFaces = 2;
		config->ApplyChanges();

		faceData = faceModule->CreateOutput();

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

		//顔のデータを更新する
		updateFaceFrame();

        // フレームを解放する
        senseManager->ReleaseFrame();

        // フレームレートを表示する
        showFps();
    }

	void updateFaceFrame(){

		// フレームデータを取得する
		const PXCCapture::Sample *sample = senseManager->QuerySample();
		if (sample) {
			// 各データを表示する
			updateColorImage(sample->color);
		}

		//SenceManagerモジュールの顔のデータを更新する
		faceData->Update();

		//検出した顔の数を取得する
		const int numFaces = faceData->QueryNumberOfDetectedFaces();

		//顔の領域を示す四角形を用意する
		PXCRectI32 faceRect = { 0 };

		//それぞれの顔ごとに情報取得および描画処理を行う
		for (int i = 0; i < numFaces; ++i) {

			//顔の情報を取得する
			auto face = faceData->QueryFaceByIndex(i);
			if (face == 0){
				continue;
			}
			
			// 顔の位置を取得:Colorで取得する
			auto detection = face->QueryDetection();
			if (detection != 0){

				//顔の大きさを取得する
				detection->QueryBoundingRect(&faceRect);
			}

			//顔の位置と大きさから、顔の領域を示す四角形を描画する
			cv::rectangle(colorImage, cv::Rect(faceRect.x, faceRect.y, faceRect.w, faceRect.h), cv::Scalar(255, 0, 0));

		}


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

    // フレームレートの表示
    void showFps()
    {
        static DWORD oldTime = ::timeGetTime();
        static int fps = 0;
        static int count = 0;

        count++;

        auto _new = ::timeGetTime();
        if ( (_new - oldTime) >= 1000 ){
            fps = count;
            count = 0;

            oldTime = _new;
        }

        std::stringstream ss;
        ss << "fps:" << fps;
        cv::putText( colorImage, ss.str(), cv::Point( 50, 50 ), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
    }

private:

    PXCSenseManager* senseManager = 0;
	cv::Mat colorImage;
    PXCFaceData* faceData = 0;
	const int DETECTION_MAXFACES = 2;    //顔を検出できる最大人数を設定

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
        RealSenseAsenseManager asenseManager;
        asenseManager.initilize();
		asenseManager.initilizeFace();
        asenseManager.run();
    }
    catch ( std::exception& ex ){
        std::cout << ex.what() << std::endl;
    }
}
