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

	io.IniFilename = nullptr;
	io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/meiryo.ttc", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	ImGui_ImplWin32_Init(GetWindowHandle());
	ImGui_ImplDX11_Init(DirectX11::GetInstance()->GetDevice(), DirectX11::GetInstance()->GetContext());

	//auto Style = ImGui::GetStyle();
	//Style.WindowRounding = 8.0f;
	//Style.FrameRounding = 6.0f;
	//Style.GrabRounding = 6.0f;
	//Style.ScrollbarRounding = 8.0f;
	//Style.WindowPadding = ImVec2(16, 16);
	//Style.FramePadding = ImVec2(10, 6);
	//Style.ItemSpacing = ImVec2(10, 10);
	//Style.ItemInnerSpacing = ImVec2(8, 8);
	//Style.Colors[ImGuiCol_WindowBg] = ImVec4(0.13f, 0.14f, 0.19f, 1.00f);
	//Style.Colors[ImGuiCol_Header] = ImVec4(0.30f, 0.35f, 0.50f, 1.00f);
	//Style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.37f, 0.42f, 0.65f, 1.00f);
	//Style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.29f, 0.42f, 1.00f);
	//Style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.33f, 0.38f, 0.53f, 1.00f);
	//Style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.45f, 0.50f, 0.75f, 1.00f);
	//Style.Colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.24f, 0.40f, 1.00f);
	//Style.Colors[ImGuiCol_TabActive] = ImVec4(0.35f, 0.40f, 0.65f, 1.00f);
}

void EditrGUI::Update(){
}

void EditrGUI::Draw(){
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

    WindowGUI();
	ImGui::Begin("Hello, Editor!");
	ImGui::Text("This is the editor GUI.");
	ImGui::End();

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
