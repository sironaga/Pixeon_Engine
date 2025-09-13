#include "Main.h"
#include "System.h"
#include "AssetsManager.h"
#include "SceneManger.h"

AssetWatcher *watcher;
bool bInGame;

int Init(const EngineConfig& InPut){
	// DirectX11‚Ì‰Šú‰»
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;

	AssetsManager::GetInstance()->Open("assets.PixAssets");
	watcher = new AssetWatcher(".", "Assets.PixAssets",
		[&]() {
			AssetsManager::GetInstance()->Open("assets.PixAssets");
		}
	);
	watcher->Start();

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
	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void UnInit(){
	watcher->Stop();
	DirectX11::DestroyInstance();
}

// “à•”ˆ—

void EditeUpdate(){
	SceneManger::GetInstance()->EditUpdate();
}

void InGameUpdate(){
	SceneManger::GetInstance()->PlayUpdate();
}

void AssetsUpdate() {

}