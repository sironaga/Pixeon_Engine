#include "Main.h"
#include "System.h"
#include "AssetsManager.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"

AssetWatcher *watcher;
GameRenderTarget* gGameRenderTarget;
bool bInGame;

int Init(const EngineConfig& InPut){
	// DirectX11�̏�����
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;
	//�@�A�Z�b�g�}�l�[�W���[�̋N��
	AssetsManager::GetInstance()->Open("assets.PixAssets");
	watcher = new AssetWatcher(".", "assets.PixAssets",
		[&]() {
			AssetsManager::GetInstance()->Open("assets.PixAssets");
		}
	);
	watcher->Start();

	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);

	bInGame = false;

	SceneManger::GetInstance()->Init();

	return 0;
}

void Update(){
	if(bInGame){
		InGameUpdate();
	}
	else{
		EditeUpdate();
	}

}

void Draw(){
	EditeDraw();
}

void UnInit(){
	watcher->Stop();
	DirectX11::DestroyInstance();
}

// ��������

void EditeUpdate(){
	SceneManger::GetInstance()->EditUpdate();
}

void InGameUpdate(){
	SceneManger::GetInstance()->PlayUpdate();
}

void EditeDraw(){
	gGameRenderTarget->Begin(DirectX11::GetInstance()->GetContext());
	SceneManger::GetInstance()->Draw();
	gGameRenderTarget->End();
}

void InGamDraw(){
	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void AssetsUpdate() {

}