// DLLのエントリーポイント
// 

#include <Windows.h>
#include "System.h"
#include "AssetsManager.h"

#define VERSION (1)


struct EngineConfig {
	HWND hWnd;
	int screenWidth;
	int screenHeight;
	const char* windowTitle;
	bool fullscreen;
	float targetFPS;
	const char* startScene;
};


extern "C" {

	// versionを取得	
	__declspec(dllexport) float SoftVersion(){
		return VERSION;
	}

	__declspec(dllexport) int SoftInit(const EngineConfig& config){
		HRESULT hr = DirectX11::GetInstance()->Init(config.hWnd, config.screenWidth, config.screenHeight, config.fullscreen);
		if (FAILED(hr)) {
			return -1;
		}
		return 0;
	}

	__declspec(dllexport) void SoftUpdate(HWND hwnd){
		AssetsManager::GetInstance()->Open("assets.PixAssets");
		AssetWatcher watcher(".", "Assets.PixAssets",
			[&]() {
				AssetsManager::GetInstance()->Open("assets.PixAssets");
			}
		);
		watcher.Start();

		watcher.Stop();
	}

	__declspec(dllexport) void SoftDraw() {

	}

	__declspec(dllexport) void SoftShutDown(){

		DirectX11::DestroyInstance();
	}

	__declspec(dllexport) bool IsEngineRunning(){

		return false;
	}

	__declspec(dllexport) void EngineProc(HWND wnd, UINT uint, WPARAM wparam, LPARAM lparam) {

	}
}