//�G�f�B�^�pGUI�N���X

#pragma once
#include <string>
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include <d3d11.h>
#include <wincodec.h>

class Object;

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

	void ShaderListWindow();
	void ExternalToolsWindow();
	void SettingWindow();
	void ShaderEditorWindow();

	bool dockNeedsReset				= false;
	bool ShowSettingsWindow			= false;
	bool ShowConsoleWindow			= false;
	bool ShowArchiveWindow			= false;
	bool ShowExternalToolsWindow	= false;
	bool ShowShaderEditorWindow		= false;
	bool ShowShaderListWindow		= false;

private:
	static ID3D11ShaderResourceView* LoadImg(const std::wstring& filename, ID3D11Device* device);
private:
	ID3D11ShaderResourceView* img;
	ID3D11ShaderResourceView* Sound;
	ID3D11ShaderResourceView* fbx;
	ID3D11ShaderResourceView* sceneIcon;
	ID3D11ShaderResourceView* folderIcon;
	ID3D11ShaderResourceView* shaderIcon;
	ID3D11ShaderResourceView* scriptIcon;
	ID3D11ShaderResourceView* JsonIcon;
	ID3D11ShaderResourceView* archiveIcon;

private:
	static EditrGUI* instance;
	Object* SelectedObject = nullptr;
private:
	EditrGUI() {}
	~EditrGUI() {}
};

