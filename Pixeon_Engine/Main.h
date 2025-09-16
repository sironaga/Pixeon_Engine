// �G���W���̊�{����
// �R�A�@���W���[��	
#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <string> 

struct EngineConfig {
	HWND hWnd;
	int screenWidth;
	int screenHeight;
	const char* windowTitle;
	bool fullscreen;
	float targetFPS;
	const char* startScene;
};

int Init(const EngineConfig& InPut );
void Update();
void Draw();
void UnInit();

// ��������
void EditeUpdate();
void InGameUpdate();
void EditeDraw();
void InGamDraw();

void OpenExplorer(const std::string& path);
std::string GetExePath();

void AssetsUpdate();
HWND GetWindowHandle();

ID3D11ShaderResourceView* GetGameRender();
