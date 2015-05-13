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
		
		//注意：表情検出を行う場合は、カラーストリームを有効化しない
		/*
		// カラーストリームを有効にする
		pxcStatus sts = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_COLOR, COLOR_WIDTH, COLOR_HEIGHT, COLOR_FPS);
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("カラーストリームの有効化に失敗しました");
		}
		*/

		initializeFace();

    }

	void initializeFace(){

		// 顔検出を有効にする
		auto sts = senseManager->EnableFace();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("顔検出の有効化に失敗しました");
		}

		// 追加：表情検出を有効にする
		sts = senseManager->EnableEmotion();
		if (sts<PXC_STATUS_NO_ERROR) {
			throw std::runtime_error("表情検出の有効化に失敗しました");
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

		// 顔検出器の設定
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
		//ここからは表出(Expression)情報の設定を参照してください
		config->QueryExpressions()->Enable();    //顔の表出情報の有効化
		config->QueryExpressions()->EnableAllExpressions();    //すべての表出情報の有効化
		config->QueryExpressions()->properties.maxTrackedFaces = 2;    //顔の表出情報の最大認識人数

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
        // 変更：trueにして顔と表情を同期しないとQueryEmotionで0が返る
        pxcStatus sts = senseManager->AcquireFrame( true );
        if ( sts < PXC_STATUS_NO_ERROR ) {
            return;
        }

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

		//追加：表情検出の結果を格納するための入れ物を用意する
		PXCEmotion::EmotionData arrData[NUM_TOTAL_EMOTIONS];

		//追加：表情を検出する
		emotionDet = senseManager->QueryEmotion();
		if (emotionDet == 0) {
			std::cout << "表情検出に失敗しました" << std::endl;
			return;
		}

		//追加：表情のラベル群
		const char *EmotionLabels[NUM_PRIMARY_EMOTIONS] = {
			"ANGER",
			"CONTEMPT",
			"DISGUST",
			"FEAR",
			"JOY",
			"SADNESS",
			"SURPRISE"
		};

		//追加：感情のラベル群
		const char *SentimentLabels[NUM_SENTIMENT_EMOTIONS] = {
			"NEGATIVE",
			"POSITIVE",
			"NEUTRAL"
		};

		//////////////////////////////////////
		//ここからは顔検出の機能
		
		//SenceManagerモジュールの顔のデータを更新する
		faceData->Update();

		//検出した顔の数を取得する
		const int numFaces = faceData->QueryNumberOfDetectedFaces();

		//それぞれの顔ごとに情報取得および描画処理を行う
		for (int i = 0; i < numFaces; ++i) {
			auto face = faceData->QueryFaceByIndex(i);
			if (face == 0){
				continue;
			}

			PXCRectI32 faceRect = { 0 };
			PXCFaceData::PoseEulerAngles poseAngle = { 0 };

			//顔の感情のデータ、および角度のデータの入れ物を用意
			PXCFaceData::ExpressionsData *expressionData;
			PXCFaceData::ExpressionsData::FaceExpressionResult expressionResult;

			// 顔の位置を取得:Colorで取得する
			auto detection = face->QueryDetection();
			if (detection != 0){
				detection->QueryBoundingRect(&faceRect);
			}

			//顔の位置と大きさから、顔の領域を示す四角形を描画する
			cv::rectangle(colorImage, cv::Rect(faceRect.x, faceRect.y, faceRect.w, faceRect.h), cv::Scalar(255, 0, 0));

			//顔のデータか表出情報のデータの情報を得る
			expressionData = face->QueryExpressions();
			if (expressionData != NULL)
			{
				//口の開き具合
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_MOUTH_OPEN, &expressionResult)){
					{
						std::stringstream ss;
						ss << "Mouth_Open:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

				//舌の出し具合
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_TONGUE_OUT, &expressionResult)){
					{
						std::stringstream ss;
						ss << "TONGUE_Out:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 40), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

				//笑顔の度合
				if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_SMILE, &expressionResult)){
					{
						std::stringstream ss;
						ss << "SMILE:" << expressionResult.intensity;
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y + faceRect.h + 65), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}

			}
			//ここまでが表出情報検出の機能
			//////////////////////////////////////

			//////////////////////////////////////
			//追加：ここからが表情(Emotion)認識

			//追加：感情のデータを得る
			emotionDet->QueryAllEmotionData(i, &arrData[0]);

			//追加：表情(PRIMARY)を推定する
			int idx_outstanding_emotion = -1;		//最終的に決定される表情の値
			bool IsSentimentPresent = false;			//表情がはっきりしているかどうか
			pxcI32 maxscoreE = -3; pxcF32 maxscoreI = 0;	//evidence,intencityのfor文での最大値(初期値は最小値)

			// arrDataに格納したすべての表情のパラメータについて見ていく
			for (int i = 0; i<NUM_PRIMARY_EMOTIONS; i++) {
				if (arrData[i].evidence < maxscoreE)  continue;	//表情の形跡(evidence)を比較
				if (arrData[i].intensity < maxscoreI) continue; //表情の強さ(intensity)を比較
				//二つの値を、ともに最も大きい場合の値へ更新
				maxscoreE = arrData[i].evidence;
				maxscoreI = arrData[i].intensity;
				idx_outstanding_emotion = i;	
			}

			//追加：表情(PRIMARY)の表示
			if (idx_outstanding_emotion != -1) {
				{
					std::stringstream ss;
					ss << "Emotion_PRIMARY:" << EmotionLabels[idx_outstanding_emotion];
					cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 40), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
				}
			}

			//表情の強さ(intensity)がある値以上の時、感情があると判断
			if (maxscoreI > 0.4){
				IsSentimentPresent = true;
			}

			//追加：感情(Sentiment)を推定する
			//表情(PRIMARY)の推定と同様なので、コメントは省略
			if (IsSentimentPresent){
				int idx_sentiment_emotion = -1;
				maxscoreE = -3; maxscoreI = 0;
				for (int i = 0; i<(10 - NUM_PRIMARY_EMOTIONS); i++) {
					if (arrData[NUM_PRIMARY_EMOTIONS + i].evidence  < maxscoreE) continue;
					if (arrData[NUM_PRIMARY_EMOTIONS + i].intensity < maxscoreI) continue;
					maxscoreE = arrData[NUM_PRIMARY_EMOTIONS + i].evidence;
					maxscoreI = arrData[NUM_PRIMARY_EMOTIONS + i].intensity;
					idx_sentiment_emotion = i;
				}
				if (idx_sentiment_emotion != -1){
					{
						std::stringstream ss;
						ss << "Emo_SENTIMENT:" << SentimentLabels[idx_sentiment_emotion];
						cv::putText(colorImage, ss.str(), cv::Point(faceRect.x, faceRect.y - 15), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2, CV_AA);
					}
				}
			}
				
			
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
        ss << fps;
        cv::putText( colorImage, ss.str(), cv::Point( 50, 50 ), cv::FONT_HERSHEY_SIMPLEX, 1.2, cv::Scalar( 0, 0, 255 ), 2, CV_AA );
    }

private:

    cv::Mat colorImage;
    PXCSenseManager* senseManager = 0;
    PXCFaceData* faceData = 0;
	PXCEmotion* emotionDet = 0; //追加：表情検出の結果を格納するための入れ物

    const int COLOR_WIDTH = 640;
	const int COLOR_HEIGHT = 480;
    const int COLOR_FPS = 30;
	static const int NUM_TOTAL_EMOTIONS = 10;		//追加：取得できる表情および感情のすべての種類の数
	static const int NUM_PRIMARY_EMOTIONS = 7;		//追加：表情の数
	static const int NUM_SENTIMENT_EMOTIONS = 3;	//追加：感情の数

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
