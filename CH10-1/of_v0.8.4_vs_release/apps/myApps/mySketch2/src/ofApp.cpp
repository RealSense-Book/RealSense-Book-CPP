#include "ofApp.h"

#include "pxccapture.h"
#include "pxchandmodule.h"
#include "pxchandconfiguration.h"
#include "pxchanddata.h"
#include "pxcsensemanager.h"

PXCSenseManager* senseManager = 0;
PXCHandModule* handAnalyzer = 0;
PXCHandData* handData = 0;

const int DEPTH_WIDTH = 640;
const int DEPTH_HEIGHT = 480;
const int DEPTH_FPS = 30;

int sides[2];
float openesses[2];
ofPoint centers[2];
ofColor circleColor;

void ofApp::initializeHandTracking()
{
	// 手の検出器を取得する
	handAnalyzer = senseManager->QueryHand();
	if (handAnalyzer == 0) {
		throw std::runtime_error("手の検出器の取得に失敗しました");
	}
	// 手のデータを作成する
	handData = handAnalyzer->CreateOutput();
	if (handData == 0) {
		throw std::runtime_error("手の検出器の作成に失敗しました");
	}
	PXCCapture::Device *device = senseManager->QueryCaptureManager()->QueryDevice();
	PXCCapture::DeviceInfo dinfo;
	device->QueryDeviceInfo(&dinfo);
	if (dinfo.model == PXCCapture::DEVICE_MODEL_IVCAM) {
		device->SetDepthConfidenceThreshold(1);
		//device->SetMirrorMode( PXCCapture::Device::MIRROR_MODE_DISABLED );
		device->SetIVCAMFilterOption(6);
	}
	// 手の検出の設定
	PXCHandConfiguration* config = handAnalyzer->CreateActiveConfiguration();
	config->EnableSegmentationImage(true);
	config->ApplyChanges();
	config->Update();
}

//--------------------------------------------------------------
void ofApp::setup(){
	ofBackground(0);
	ofSetFrameRate(60);

	// 1,000,000 particles
	unsigned w = 1000;
	unsigned h = 1000;

	particles.init(w, h);

	// initial positions
	// use new to allocate 4,000,000 floats on the heap rather than
	// the stack
	float* particlePosns = new float[w * h * 4];
	for (unsigned y = 0; y < h; ++y)
	{
		for (unsigned x = 0; x < w; ++x)
		{
			unsigned idx = y * w + x;
			particlePosns[idx * 4] = 400.f * x / (float)w - 200.f; // particle x
			particlePosns[idx * 4 + 1] = 400.f * y / (float)h - 200.f; // particle y
			particlePosns[idx * 4 + 2] = 0.f; // particle z
			particlePosns[idx * 4 + 3] = 0.f; // dummy
		}
	}
	particles.loadDataTexture(ofxGpuParticles::POSITION, particlePosns);
	delete[] particlePosns;

	// initial velocities
	particles.zeroDataTexture(ofxGpuParticles::VELOCITY);

	// listen for update event to set additonal update uniforms
	ofAddListener(particles.updateEvent, this, &ofApp::onParticlesUpdate);
	circleColor.r = 255;
	circleColor.g = 0;
	circleColor.b = 0;
	circleColor.a = 128;

	senseManager = PXCSenseManager::CreateInstance();
	if (senseManager == 0) {
		throw std::runtime_error("SenseManagerの生成に失敗しました");
	}

	// Depthストリームを有効にする
	auto sts = senseManager->EnableStream(PXCCapture::StreamType::STREAM_TYPE_DEPTH,
		DEPTH_WIDTH, DEPTH_HEIGHT, DEPTH_FPS);
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("Depthストリームの有効化に失敗しました");
	}
	// 手の検出を有効にする
	sts = senseManager->EnableHand();
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("手の検出の有効化に失敗しました");
	}
	// パイプラインを初期化する
	sts = senseManager->Init();
	if (sts < PXC_STATUS_NO_ERROR) {
		throw std::runtime_error("パイプラインの初期化に失敗しました");
	}

	initializeHandTracking();

}

// set any update uniforms in this function
void ofApp::onParticlesUpdate(ofShader& shader)
{
	// 手の位置をシェーダーの座標系に合わせて計算
	ofVec3f mouse(-1.0f * (centers[0].x * ofGetWidth() / DEPTH_WIDTH - .5f * ofGetWidth()), .5f * ofGetHeight() - (centers[0].y * ofGetHeight() / DEPTH_HEIGHT), 0.f);

	// 計算した手の位置をシェーダーにセット
	shader.setUniform3fv("mouse", mouse.getPtr());
	shader.setUniform1f("elapsed", ofGetLastFrameTime());

	// 手の開閉値でパーティクルの集まり具合を変化させる
	float openForRad;
	if (openesses[0] < 10.0f){
		openForRad = 10.0f;
	}
	else if (openesses[0] > 100.0f){
		openForRad = 100.0f;
	}
	else{
		openForRad = openesses[0];
	}
	openForRad = 1.0f - (openForRad / 100.0f) + 0.01f;

	shader.setUniform1f("radiusSquared", 200.f * 200.f * 10.0f * openForRad);
}

void ofApp::updateHandFrame(){

	handData->Update();
	// 検出した手の数を取得する
	auto numOfHands = handData->QueryNumberOfHands();
	for (int i = 0; i < numOfHands; i++) {
		// 手を取得する
		pxcUID handID;
		PXCHandData::IHand* hand;
		auto sts = handData->QueryHandData(
			PXCHandData::AccessOrderType::ACCESS_ORDER_BY_ID, i, hand);
		if (sts < PXC_STATUS_NO_ERROR) {
			continue;
		}

		auto side = hand->QueryBodySide();
		auto openness = hand->QueryOpenness();
		auto center = hand->QueryMassCenterImage();

		if (i < 2){
			sides[i] = side;
			openesses[i] = openness;
			centers[i].x = center.x;
			centers[i].y = center.y;
		}
	}
}

//--------------------------------------------------------------
void ofApp::update(){

	pxcStatus sts = senseManager->AcquireFrame(false);
	if (sts < PXC_STATUS_NO_ERROR) {
		return;
	}
	// 手のデータを更新する
	updateHandFrame();
	// フレームを解放する
	senseManager->ReleaseFrame();

	ofSetWindowTitle(ofToString(ofGetFrameRate(), 2));
	particles.update();
}

//--------------------------------------------------------------
void ofApp::draw(){
	cam.begin();
	ofEnableBlendMode(OF_BLENDMODE_ADD);
	particles.draw();
	ofDisableBlendMode();

	// 円の位置の計算
	ofVec3f handPoint(-1.0f * (centers[0].x * ofGetWidth() / DEPTH_WIDTH - .5f * ofGetWidth()), .5f * ofGetHeight() - (centers[0].y * ofGetHeight() / DEPTH_HEIGHT), 0.f);
	// 円の色の指定
	ofSetColor(circleColor);
	// アウトラインのみ描画
	ofNoFill();
	// 円の描画
	ofCircle(handPoint.x, handPoint.y, 2.0f * openesses[0]);

	cam.end();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){

}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}
