﻿// EditrGUI.cpp
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#pragma comment(lib, "windowscodecs.lib")

EditrGUI* EditrGUI::instance = nullptr;

EditrGUI* EditrGUI::GetInstance()
{
    if (instance == nullptr) {
        instance = new EditrGUI();
    }
    return instance;
}

void EditrGUI::DestroyInstance(){
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void EditrGUI::Init(){
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    io.IniFilename = nullptr;

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    ImGui_ImplWin32_Init(GetWindowHandle());
    ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // タブを完全フラット＆ボーダーレスに
    ImVec4 flatTabColor = ImVec4(0.18f, 0.20f, 0.23f, 1.00f); // 基本のタブ背景色
    ImVec4 flatTabActive = ImVec4(0.20f, 0.22f, 0.25f, 1.00f); // アクティブタブ（ほんのり違い）
    ImVec4 flatTabHovered = ImVec4(0.23f, 0.25f, 0.28f, 1.00f); // ホバー（やや明るく）

    colors[ImGuiCol_Tab] = flatTabColor;
    colors[ImGuiCol_TabUnfocused] = flatTabColor;
    colors[ImGuiCol_TabUnfocusedActive] = flatTabColor;
    colors[ImGuiCol_TabActive] = flatTabColor; // アクティブ時も同じ色に
    colors[ImGuiCol_TabHovered] = flatTabColor; // ホバー時も同じ色に
    colors[ImGuiCol_TabActive] = flatTabActive;
    colors[ImGuiCol_TabHovered] = flatTabHovered;

    // タブとタブのボーダーを完全に消す
    colors[ImGuiCol_Border] = flatTabColor;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // タブの角丸を無しに
    style.TabRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.WindowRounding = 0.0f;

    style.TabBorderSize = 0.0f;

    // タブの内側余白を抑えめにして高さも下げる
    style.FramePadding = ImVec2(12, 4);
    style.ItemSpacing = ImVec2(6, 2);

    // タブのテキスト色（アクティブのみ白寄り・非アクティブはグレー寄りで差をつける）
    colors[ImGuiCol_Text] = ImVec4(0.88f, 0.90f, 0.94f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.48f, 0.54f, 1.00f);

	img         = LoadImg(L"SceneRoot/Editor/texture/img.png", DirectX11::GetInstance()->GetDevice());
	Sound       = LoadImg(L"SceneRoot/Editor/texture/Sound.png", DirectX11::GetInstance()->GetDevice());
	fbx         = LoadImg(L"SceneRoot/Editor/texture/fbx.png", DirectX11::GetInstance()->GetDevice());
	sceneIcon   = LoadImg(L"SceneRoot/Editor/texture/Scene.png", DirectX11::GetInstance()->GetDevice());
	folderIcon  = LoadImg(L"SceneRoot/Editor/texture/File.png", DirectX11::GetInstance()->GetDevice());
	shaderIcon  = LoadImg(L"SceneRoot/Editor/texture/HLSL.png", DirectX11::GetInstance()->GetDevice());
	scriptIcon  = LoadImg(L"SceneRoot/Editor/texture/Script.png", DirectX11::GetInstance()->GetDevice());
	JsonIcon    = LoadImg(L"SceneRoot/Editor/texture/Json.png", DirectX11::GetInstance()->GetDevice());
    archiveIcon = LoadImg(L"SceneRoot/Editor/texture/Archive.png", DirectX11::GetInstance()->GetDevice());
	ExeIcon     = LoadImg(L"SceneRoot/Editor/texture/Exe.png", DirectX11::GetInstance()->GetDevice());
}

void EditrGUI::Update(){
}

void EditrGUI::Draw()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    WindowGUI();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

std::string EditrGUI::ShiftJISToUTF8(const std::string& str)
{
    if (str.empty()) return {};

    // Shift-JIS → UTF-16
    int lenW = MultiByteToWideChar(932, 0, str.data(), (int)str.size(), nullptr, 0);
    if (lenW <= 0) return {};

    std::wstring wstr(lenW, 0);
    MultiByteToWideChar(932, 0, str.data(), (int)str.size(), &wstr[0], lenW);

    // UTF-16 → UTF-8
    int lenU8 = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, nullptr, 0, nullptr, nullptr);
    if (lenU8 <= 0) return {};

    std::string u8str(lenU8, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), lenW, &u8str[0], lenU8, nullptr, nullptr);

    return u8str;
}

void EditrGUI::WindowGUI()
{
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("MainDockspace", nullptr, window_flags);

    // メニューバー
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(ShiftJISToUTF8("ファイル").c_str()))
        {
            ImGui::MenuItem(ShiftJISToUTF8("新規シーン").c_str());
            ImGui::MenuItem(ShiftJISToUTF8("開く...").c_str());
            ImGui::MenuItem(ShiftJISToUTF8("保存").c_str());
            if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーリスト").c_str())) ShowShaderListWindow = true;
            if (ImGui::MenuItem(ShiftJISToUTF8("環境設定").c_str())) ShowSettingsWindow = true;
            ImGui::Separator();
            if (ImGui::MenuItem(ShiftJISToUTF8("終了").c_str())) SetRun(false);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ShiftJISToUTF8("編集").c_str()))
        {
            ImGui::MenuItem(ShiftJISToUTF8("取り消し").c_str());
            ImGui::MenuItem(ShiftJISToUTF8("やり直し").c_str());
            ImGui::Separator();
            // レイアウト初期化
            if (ImGui::MenuItem(ShiftJISToUTF8("レイアウトを初期状態に戻す").c_str())){
                ImGui::GetIO().IniFilename = nullptr;
                ImGui::LoadIniSettingsFromMemory("");
                dockNeedsReset = true;
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(ShiftJISToUTF8("ツール").c_str()))
        {
            if (ImGui::MenuItem(ShiftJISToUTF8("アーカイブ化").c_str())) ShowArchiveWindow = true;
            if (ImGui::MenuItem(ShiftJISToUTF8("シェーダーエディタ").c_str())) ShowShaderEditorWindow = true;
			
            if (ImGui::MenuItem(ShiftJISToUTF8("フォルダ").c_str())) {
                std::string Path = GetExePath();
                Path = RemoveExeFromPath(Path);
				OpenExplorer(Path);
            }

            if (ImGui::MenuItem(ShiftJISToUTF8("外部ツール").c_str())) ShowExternalToolsWindow = true;
            
            ImGui::EndMenu();
		}
        ImGui::EndMenuBar();
    }
	// 環境設定ウィンドウ
    if (ShowSettingsWindow) SettingWindow();
    // ツールウインドウ
    if (ShowArchiveWindow)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin(ShiftJISToUTF8("アーカイブ化").c_str(), &ShowArchiveWindow, flags)){

			ImGui::Text(ShiftJISToUTF8("ツールパス:").c_str());
			ImGui::SameLine();
            ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetPackingToolFilePath()).c_str());
			ImGui::Text(ShiftJISToUTF8("アセットフォルダ:").c_str());
			ImGui::SameLine();
			ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetAssetsFilePath()).c_str());
			ImGui::Text(ShiftJISToUTF8("アーカイブ出力先:").c_str());
			ImGui::SameLine();
			ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetArchiveFilePath() + "/assets.PixAssets").c_str());
			ImGui::Separator();
			ImGui::Text(ShiftJISToUTF8("説明").c_str());
			ImGui::TextWrapped(ShiftJISToUTF8("アセットフォルダ内のファイルを一つのアーカイブファイルにまとめます。アーカイブ化には時間がかかる場合があります。").c_str());
            if (ImGui::Button(ShiftJISToUTF8("アーカイブ化実行").c_str(), ImVec2(120, 0))) {
                bool OK = false;
                OK = RunArchiveTool(SettingManager::GetInstance()->GetPackingToolFilePath(),
                    SettingManager::GetInstance()->GetAssetsFilePath(),
                    SettingManager::GetInstance()->GetArchiveFilePath() + "/assets.PixAssets");
                if(OK)
					MessageBoxA(NULL, "アーカイブ化に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				else
                    MessageBoxA(NULL, "アーカイブ化に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
            }
        }
        ImGui::End();
    }

    ExternalToolsWindow();
    
    // DockSpaceを作成
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

    // Dock初期化
    static bool dock_init = false;
    if (!dock_init || dockNeedsReset) {
        dock_init = true;
        dockNeedsReset = false;
        ImGui::DockBuilderRemoveNode(dockspace_id); // DockSpaceリセット
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_id_right;
        ImGuiID dock_id_bottom;
        ImGuiID dock_id_left;

        // 右にInspector
        ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, &dock_id_right, &dock_main_id);
        // 下にContentDrawer
        ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, &dock_id_bottom, &dock_main_id);
        // 左にHierarchy
        ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.18f, &dock_id_left, &dock_main_id);

        ImGui::DockBuilderDockWindow(ShiftJISToUTF8("ゲームビュー").c_str(), dock_main_id);
        ImGui::DockBuilderDockWindow(ShiftJISToUTF8("インスペクター").c_str(), dock_id_right);
        ImGui::DockBuilderDockWindow(ShiftJISToUTF8("コンテンツドロワー").c_str(), dock_id_bottom);
        ImGui::DockBuilderDockWindow(ShiftJISToUTF8("ヒエラルキー").c_str(), dock_id_left);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();
    ImGui::PopStyleVar();

    ShowContentDrawer();
    ShaderEditorWindow();
	ShaderListWindow();
    ShowHierarchy();
    ShowInspector();
    ShowGameView();
    ShowConsole();
}

void EditrGUI::ShowGameView()
{
  ImGui::Begin(ShiftJISToUTF8("ゲームビュー").c_str());

    // --- 上部にコントロールバー ---
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

	bool isGamePlaying = false; // ゲーム実行中かどうかのフラグ

    // 再生・停止ボタン
    if (!isGamePlaying) {
        if (ImGui::Button(ShiftJISToUTF8("再生").c_str(), ImVec2(70, 0))) {
            isGamePlaying = true;
            // --- ここでゲーム実行処理を呼ぶ ---
        }
    } else {
        if (ImGui::Button(ShiftJISToUTF8("停止").c_str(), ImVec2(70, 0))) {
            isGamePlaying = false;
            // --- ここでゲーム停止処理を呼ぶ ---
        }
    }

    ImGui::PopStyleVar(2);

    ImGui::Separator();

    // --- ゲーム画面（プレビュー） ---
    ID3D11ShaderResourceView* srv = GetGameRender();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (srv) {
        ImGui::Image((ImTextureID)srv, size);
    }
    else {
        ImGui::Text("SRVがnullです");
    }
    ImGui::End();
}

void EditrGUI::ShowConsole(){
	if (!ShowConsoleWindow)return;
    ImGui::Begin(ShiftJISToUTF8("コンソール").c_str());



	ImGui::End();
}

void EditrGUI::ShaderListWindow(){
	if (!ShowShaderListWindow)return;
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin(ShiftJISToUTF8("シェーダーリスト").c_str(), &ShowShaderListWindow, flags)) {
        static int shaderListType = 0;
        const char* shaderListTypes[] = { "Vertex Shader", "Pixel Shader" };
        ImGui::Combo(ShiftJISToUTF8("シェーダーリスト").c_str(), &shaderListType, shaderListTypes, IM_ARRAYSIZE(shaderListTypes));
		ImGui::Separator();
		std::vector<std::string> ShaderList;
        ShaderList.clear();
		if (shaderListType == 0)
			ShaderList = ShaderManager::GetInstance()->GetShaderList("VS");
		else
			ShaderList = ShaderManager::GetInstance()->GetShaderList("PS");
		for (const auto& shaderName : ShaderList) {
			ImGui::Text(ShiftJISToUTF8(shaderName).c_str());
		}
        ImGui::End();
    }
}

void EditrGUI::ExternalToolsWindow(){
	if (!ShowExternalToolsWindow)return;
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin(ShiftJISToUTF8("外部ツール管理").c_str(), &ShowExternalToolsWindow, flags)) {
		ImGui::Text(ShiftJISToUTF8("外部ツール管理ウインドウです").c_str());
		std::string ExternelTool = SettingManager::GetInstance()->GetExternelToolPath();
		ImGui::Text(ShiftJISToUTF8("外部ツールフォルダ:").c_str());
		ImGui::SameLine();
		ImGui::Text(ShiftJISToUTF8(ExternelTool).c_str());
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("外部ツールフォルダにある実行ファイルを一覧表示します。").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("実行ファイルをクリックすると外部ツールが起動します。").c_str());
		ImGui::Separator();

        ImGui::End();
    }

}

void EditrGUI::SettingWindow()
{
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin(ShiftJISToUTF8("環境設定").c_str(), &ShowSettingsWindow, flags)){

		ImGui::Text(ShiftJISToUTF8("パスの設定").c_str());
		ImGui::Separator();
		std::string assetsPath = SettingManager::GetInstance()->GetAssetsFilePath();
		char assetsBuffer[256];
		strncpy_s(assetsBuffer, assetsPath.c_str(), sizeof(assetsBuffer));
        if (ImGui::InputText(ShiftJISToUTF8("アセットフォルダ").c_str(), assetsBuffer, sizeof(assetsBuffer)))SettingManager::GetInstance()->SetAssetsFilePath(assetsBuffer);
		ImGui::Text(ShiftJISToUTF8("* アセットフォルダパスを変えた場合再起動してください").c_str());
		std::string archivePath = SettingManager::GetInstance()->GetArchiveFilePath();
		char archiveBuffer[256];
        strncpy_s(archiveBuffer, archivePath.c_str(), sizeof(archiveBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("アーカイブフォルダ").c_str(), archiveBuffer, sizeof(archiveBuffer)))SettingManager::GetInstance()->SetArchiveFilePath(archiveBuffer);
		std::string scenePath = SettingManager::GetInstance()->GetSceneFilePath();
        char sceneBuffer[256];
		strncpy_s(sceneBuffer, scenePath.c_str(), sizeof(sceneBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("シーンフォルダ").c_str(), sceneBuffer, sizeof(sceneBuffer)))SettingManager::GetInstance()->SetSceneFilePath(sceneBuffer);
		std::string shaderPath = SettingManager::GetInstance()->GetShaderFilePath();
		char shaderBuffer[256];
		strncpy_s(shaderBuffer, shaderPath.c_str(), sizeof(shaderBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("シェーダーフォルダ").c_str(), shaderBuffer, sizeof(shaderBuffer)))SettingManager::GetInstance()->SetShaderFilePath(shaderBuffer);
		std::string csoPath = SettingManager::GetInstance()->GetCSOFilePath();
		char csoBuffer[256];
		strncpy_s(csoBuffer, csoPath.c_str(), sizeof(csoBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("CSOフォルダ").c_str(), csoBuffer, sizeof(csoBuffer)))SettingManager::GetInstance()->SetCSOFilePath(csoBuffer);
        std::string PackingTool = SettingManager::GetInstance()->GetPackingToolFilePath();
        char PackingToolBuffer[256];
        strncpy_s(PackingToolBuffer, PackingTool.c_str(), sizeof(PackingToolBuffer));
        if (ImGui::InputText(ShiftJISToUTF8("PackingTool").c_str(), PackingToolBuffer, sizeof(PackingToolBuffer)))SettingManager::GetInstance()->SetPackingToolFilePath(PackingToolBuffer);
        
		char ExternelToolBuffer[256];
		std::string ExternelTool = SettingManager::GetInstance()->GetExternelToolPath();
		strncpy_s(ExternelToolBuffer, ExternelTool.c_str(), sizeof(ExternelToolBuffer));
		if (ImGui::InputText(ShiftJISToUTF8("外部ツールフォルダ").c_str(), ExternelToolBuffer, sizeof(ExternelToolBuffer)))SettingManager::GetInstance()->SetExternelToolPath(ExternelToolBuffer);

		ImGui::Text(ShiftJISToUTF8("レンダリング設定").c_str());
		ImGui::Separator();
		bool bZBuffer = SettingManager::GetInstance()->GetZBuffer();
		if (ImGui::Checkbox(ShiftJISToUTF8("Zバッファを有効にする").c_str(), &bZBuffer))SettingManager::GetInstance()->SetZBuffer(bZBuffer);

		int autoSaveInterval = SettingManager::GetInstance()->GetAutoSaveInterval();
        if (ImGui::InputInt(ShiftJISToUTF8("自動保存間隔（分）").c_str(), &autoSaveInterval)) {
            if (autoSaveInterval < 1) autoSaveInterval = 1;
            SettingManager::GetInstance()->SetAutoSaveInterval(autoSaveInterval);
        }

        ImGui::Text(ShiftJISToUTF8("バックグラウンドカラーを変更").c_str());
        DirectX::XMFLOAT4 Color;
        Color = SettingManager::GetInstance()->GetBackgroundColor();
		float color[4] = { Color.x, Color.y, Color.z, Color.w };
        if (ImGui::ColorEdit4(ShiftJISToUTF8("背景色").c_str(), color)) {
            SettingManager::GetInstance()->SetBackgroundColor(DirectX::XMFLOAT4(color[0], color[1], color[2], color[3]));
		}



    }
    ImGui::End();
}

void EditrGUI::ShaderEditorWindow(){
    if (!ShowShaderEditorWindow)return;
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
    if (
        ImGui::Begin(ShiftJISToUTF8("シェーダー作成").c_str(), &ShowShaderEditorWindow, flags)) {
		ImGui::Text(ShiftJISToUTF8("シェーダーエディターウインドウです").c_str());
		ImGui::Text(ShiftJISToUTF8("シェーダーファイルフォルダ:").c_str());
		ImGui::SameLine();
		ImGui::Text(ShiftJISToUTF8(SettingManager::GetInstance()->GetShaderFilePath()).c_str());
		ImGui::Separator();
		ImGui::Text(ShiftJISToUTF8("説明").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("シェーダーファイルを編集した後、保存すると自動的にコンパイルされます。").c_str());
		ImGui::TextWrapped(ShiftJISToUTF8("コンパイルエラーが発生した場合、メッセージボックスで通知されます。").c_str());
		static char shaderName[128] = "";
		ImGui::InputText(ShiftJISToUTF8("シェーダー名").c_str(), shaderName, sizeof(shaderName));
		static int shaderType = 0;
		const char* shaderTypes[] = { "Vertex Shader", "Pixel Shader" };
		ImGui::Combo(ShiftJISToUTF8("シェーダータイプ").c_str(), &shaderType, shaderTypes, IM_ARRAYSIZE(shaderTypes));
        if (ImGui::Button(ShiftJISToUTF8("新規シェーダーファイル作成").c_str(), ImVec2(180, 0))) {
            if (strlen(shaderName) == 0) {
                MessageBoxA(NULL, "シェーダー名を入力してください。", "エラー", MB_OK | MB_ICONERROR);
            }
            else {
                std::string name = shaderName;
                if (shaderType == 0) {
                    name = "VS_" + name;
                    if (ShaderManager::GetInstance()->CreateHLSLTemplate(name, "VS")) {
                        MessageBoxA(NULL, "頂点シェーダーのテンプレートを作成しました。", "成功", MB_OK | MB_ICONINFORMATION);
                    }
                    else {
                        MessageBoxA(NULL, "シェーダーファイルの作成に失敗しました。", "エラー", MB_OK | MB_ICONERROR);
                    }
                }
                else {
                    name = "PS_" + name;
                    if (ShaderManager::GetInstance()->CreateHLSLTemplate(name, "PS")) {
                        MessageBoxA(NULL, "ピクセルシェーダーのテンプレートを作成しました。", "成功", MB_OK | MB_ICONINFORMATION);
                    }
                    else {
                        MessageBoxA(NULL, "シェーダーファイルの作成に失敗しました。", "エラー", MB_OK | MB_ICONERROR);
                    }
                }
            }
		}
		ImGui::End();
    }
}

ID3D11ShaderResourceView* EditrGUI::LoadImg(const std::wstring& filename, ID3D11Device* device)
{
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(filename.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT width = 0, height = 0;
    if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);

    std::vector<BYTE> buffer;
    if (SUCCEEDED(hr)) {
        buffer.resize(width * height * 4);
        hr = converter->CopyPixels(nullptr, width * 4, (UINT)buffer.size(), buffer.data());
    }
    if (SUCCEEDED(hr)) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initData = { buffer.data(), width * 4, 0 };
        ID3D11Texture2D* tex = nullptr;
        hr = device->CreateTexture2D(&desc, &initData, &tex);
        if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(tex, nullptr, &srv);
        if (tex) tex->Release();
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    CoUninitialize();

    return srv;
}

