/*
* Engine StartUp
* �����: �A�L�m
* �R�[�f�B���O�K��: Google C++ Style Guide
* https://ttsuki.github.io/styleguide/cppguide.ja.html
*/

#include "System.h"
#include "EngineManager.h"
#include "IMGUI/imgui_impl_win32.h" 
#include "IMGUI/imgui_impl_dx11.h"
#include "StartUp.h"

// �o�[�W����
#define VERSION (100)
int g_nScreenWidth	= 1920;
int g_nScreenHeight = 1080;
bool g_bInit		= false;
bool g_bRun			= false;

extern "C" {

	// version���擾	
	__declspec(dllexport) float SoftVersion(){
		return VERSION;
	}

	__declspec(dllexport) int SoftInit(const EngineManager::EngineConfig& config){
		int nResult = 0;
		nResult = EngineManager::GetInstance()->Init(config);
		g_nScreenHeight = config.screenHeight;
		g_nScreenWidth	= config.screenWidth;
		g_bRun	= true;
		g_bInit = true;
		return nResult;
	}

	__declspec(dllexport) void SoftUpdate(HWND hwnd){
		EngineManager::GetInstance()->Update();
	}

	__declspec(dllexport) void SoftDraw() {
		EngineManager::GetInstance()->Draw();
	}

	__declspec(dllexport) void SoftShutDown(){
		EngineManager::GetInstance()->UnInit();
		EngineManager::DeleteInstance();
	}

	__declspec(dllexport) bool IsEngineRunning(){
		return g_bRun;
	}

	__declspec(dllexport) void EngineProc(HWND wnd, UINT uint, WPARAM wparam, LPARAM lparam) {
		ImGui_ImplWin32_WndProcHandler(wnd, uint, wparam, lparam);
		switch (uint) {
		case WM_SIZE:
			if (g_bInit && wparam != SIZE_MINIMIZED) {
				UINT width = LOWORD(lparam);
				UINT height = HIWORD(lparam);
				g_nScreenHeight = height;
				g_nScreenWidth = width;
				DirectX11::GetInstance()->OnResize(width, height);
			}
			break;
		}
	}
}

void SetRun(bool run){
	g_bRun = run;
}
