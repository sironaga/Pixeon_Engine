// EditrGUI.cpp
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"

EditrGUI* EditrGUI::instance = nullptr;

EditrGUI* EditrGUI::GetInstance()
{
    if (instance == nullptr) {
        instance = new EditrGUI();
    }
    return instance;
}

void EditrGUI::DestroyInstance()
{
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

void EditrGUI::Init()
{
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

}

void EditrGUI::Update()
{
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
            ImGui::Separator();
            if (ImGui::MenuItem(ShiftJISToUTF8("終了").c_str())) {
				SetRun(false);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ShiftJISToUTF8("設定").c_str()))
        {
            if (ImGui::MenuItem(ShiftJISToUTF8("環境設定").c_str()))
            {
				ShowSettingsWindow = true;
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(ShiftJISToUTF8("編集").c_str()))
        {
            ImGui::MenuItem(ShiftJISToUTF8("取り消し").c_str());
            ImGui::MenuItem(ShiftJISToUTF8("やり直し").c_str());
            ImGui::Separator();
            // レイアウト初期化
            if (ImGui::MenuItem(ShiftJISToUTF8("レイアウトを初期状態に戻す").c_str()))
            {
                ImGui::GetIO().IniFilename = nullptr;
                ImGui::LoadIniSettingsFromMemory("");
                dockNeedsReset = true; // 次のフレームでDockレイアウト初期化
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu(ShiftJISToUTF8("ツール").c_str()))
        {
            if (ImGui::MenuItem(ShiftJISToUTF8("アーカイブ化").c_str())) {
                ShowArchiveWindow = true;
            }
			if (ImGui::MenuItem(ShiftJISToUTF8("コンソール").c_str()))ShowConsoleWindow = !ShowConsoleWindow;
            if (ImGui::MenuItem(ShiftJISToUTF8("フォルダ").c_str())) {
                std::string Path = GetExePath();
                Path = RemoveExeFromPath(Path);
				OpenExplorer(Path);
            }
            ImGui::EndMenu();
		}
        ImGui::EndMenuBar();
    }
	// 環境設定ウィンドウ
    if (ShowSettingsWindow)
    {
        SettingWindow();
    }
    // ツールウインドウ
    if (ShowArchiveWindow)
    {
        ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;
        if (ImGui::Begin(ShiftJISToUTF8("アーカイブ化").c_str(), &ShowArchiveWindow, flags)){
            if (ImGui::Button(ShiftJISToUTF8("アーカイブ化実行").c_str(), ImVec2(120, 0))) {
                bool OK = false;
                OK = CallAssetPacker(SettingManager::GetInstance()->GetPackingToolFilePath(),"../Assets", "assets.PixAssets");
                if(OK)
					MessageBoxA(NULL, "アーカイブ化に成功しました。", "成功", MB_OK | MB_ICONINFORMATION);
				else
                    MessageBoxA(NULL, "アーカイブ化に失敗しました。", "失敗", MB_OK | MB_ICONERROR);
            }
        }
        ImGui::End();
    }

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
    ShowHierarchy();
    ShowInspector();
    ShowGameView();
    ShowConsole();
}

void EditrGUI::ShowContentDrawer()
{
    ImGui::Begin(ShiftJISToUTF8("コンテンツドロワー").c_str());
    ImGui::Text(ShiftJISToUTF8("ここにアセット一覧を表示します。").c_str());
    ImGui::End();
}

void EditrGUI::ShowHierarchy()
{
    ImGui::Begin(ShiftJISToUTF8("ヒエラルキー").c_str());
    ImGui::Text(ShiftJISToUTF8("ここにシーンのオブジェクト階層を表示します。").c_str());
    ImGui::End();
}

void EditrGUI::ShowInspector()
{
    ImGui::Begin(ShiftJISToUTF8("インスペクター").c_str());
    ImGui::Text(ShiftJISToUTF8("ここに選択オブジェクトの詳細情報を表示します。").c_str());
    ImGui::End();
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
        std::string PackingTool = SettingManager::GetInstance()->GetPackingToolFilePath();
        char PackingToolBuffer[256];
        strncpy_s(PackingToolBuffer, PackingTool.c_str(), sizeof(PackingToolBuffer));
        if (ImGui::InputText(ShiftJISToUTF8("PackingTool").c_str(), PackingToolBuffer, sizeof(PackingToolBuffer)))SettingManager::GetInstance()->SetPackingToolFilePath(PackingToolBuffer);
        
		ImGui::Text(ShiftJISToUTF8("レンダリング設定").c_str());
		ImGui::Separator();
		bool bZBuffer = SettingManager::GetInstance()->GetZBuffer();
		if (ImGui::Checkbox(ShiftJISToUTF8("Zバッファを有効にする").c_str(), &bZBuffer))SettingManager::GetInstance()->SetZBuffer(bZBuffer);

		int autoSaveInterval = SettingManager::GetInstance()->GetAutoSaveInterval();
        if (ImGui::InputInt(ShiftJISToUTF8("自動保存間隔（分）").c_str(), &autoSaveInterval)) {
            if (autoSaveInterval < 1) autoSaveInterval = 1;
            SettingManager::GetInstance()->SetAutoSaveInterval(autoSaveInterval);
        }


    }
    ImGui::End();
}
