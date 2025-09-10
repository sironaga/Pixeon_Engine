// DLLのエントリーポイント
// 

#include <Windows.h>

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

	}

	__declspec(dllexport) void SoftUpdate(HWND hwnd){

	}

	__declspec(dllexport) void SoftDraw() {

	}

	__declspec(dllexport) void SoftShutDown(){

	}

	__declspec(dllexport) bool IsEngineRunning(){

	}

	__declspec(dllexport) void EngineProc(HWND wnd, UINT uint, WPARAM wparam, LPARAM lparam) {

	}
}