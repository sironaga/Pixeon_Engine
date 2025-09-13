// DLLのエントリーポイント
// コアモジュール

#include "System.h"
#include "Main.h"
#include "AssetsManager.h"

#define VERSION (1)

extern "C" {

	// versionを取得	
	__declspec(dllexport) float SoftVersion(){
		return VERSION;
	}

	__declspec(dllexport) int SoftInit(const EngineConfig& config){
		int nResult = 0;
		nResult = Init(config);
		return nResult;
	}

	__declspec(dllexport) void SoftUpdate(HWND hwnd){
		Update();
	}

	__declspec(dllexport) void SoftDraw() {
		Draw();
	}

	__declspec(dllexport) void SoftShutDown(){
		UnInit();
	}

	__declspec(dllexport) bool IsEngineRunning(){

		return false;
	}

	__declspec(dllexport) void EngineProc(HWND wnd, UINT uint, WPARAM wparam, LPARAM lparam) {

	}
}