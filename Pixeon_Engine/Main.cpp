#include "Main.h"
#include "System.h"
#include "AssetsManager.h"

AssetWatcher *watcher;

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

	return 0;
}

void Update(){


}

void Draw(){
}

void UnInit(){
	watcher->Stop();
	DirectX11::DestroyInstance();
}

// “à•”ˆ—

void EditeUpdate(){

}

void InGameUpdate(){
}

void AssetsUpdate() {

}