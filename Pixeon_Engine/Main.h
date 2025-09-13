// エンジンの基本処理
// コア　モジュール	
#pragma once

#include <Windows.h>

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

// 内部処理
void EditeUpdate();
void InGameUpdate();

void AssetsUpdate();

bool bInGame;