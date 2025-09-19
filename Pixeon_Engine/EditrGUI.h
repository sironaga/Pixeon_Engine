//エディタ用GUIクラス

#pragma once
#include <string>
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include <d3d11.h>
#include <wincodec.h>
#include <string>

class EditrGUI
{
public:
	static EditrGUI* GetInstance();
	static void DestroyInstance();

	void Init();
	void Update();
	void Draw();

public:
	std::string ShiftJISToUTF8(const std::string& str);
	ImTextureID GetAssetIcon(EditrGUI* gui, const std::string& name);
private:
	void WindowGUI();

	void ShowContentDrawer();
	void ShowHierarchy();
	void ShowInspector();
	void ShowGameView();
	void ShowConsole();

	void SettingWindow();

	bool dockNeedsReset = false;
	bool ShowSettingsWindow = false;
	bool ShowConsoleWindow = false;
	bool ShowArchiveWindow = false;


private:
	static ID3D11ShaderResourceView* LoadImg(const std::wstring& filename, ID3D11Device* device);
private:
	ID3D11ShaderResourceView* img;
	ID3D11ShaderResourceView* Sound;
	ID3D11ShaderResourceView* fbx;
private:
	static EditrGUI* instance;
private:
	EditrGUI() {}
	~EditrGUI() {}
};

