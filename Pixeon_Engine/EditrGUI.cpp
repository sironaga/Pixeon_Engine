#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"

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

void EditrGUI::Init(){
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // iniファイルを無効化してプログラムで制御
    io.IniFilename = nullptr;

    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    ImGui_ImplWin32_Init(GetWindowHandle());
    ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

}

void EditrGUI::Update(){
}

void EditrGUI::Draw(){
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    WindowGUI();


    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void EditrGUI::WindowGUI(){
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
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene")) {
            }
            if (ImGui::MenuItem("Open...")) {
            }
            if (ImGui::MenuItem("Save")) {
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings"))
        {
            if (ImGui::MenuItem("Preferences")) {
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Editor"))
        {
            if (ImGui::MenuItem("Toggle Console")) {
            }
            if (ImGui::MenuItem("Toggle Inspector")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
