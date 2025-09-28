#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "SettingManager.h"
#include <filesystem>
#include <vector>
#include <string>
#include <Windows.h> // ShellExecute�p

static std::string selectedExt = "";
static std::filesystem::path currentDir;

ImTextureID EditrGUI::GetAssetIcon(EditrGUI* gui, const std::string& name) {
    size_t dot = name.find_last_of('.');
    std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
    if (ext == ".png" || ext == ".jpg") return (ImTextureID)img;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return (ImTextureID)Sound;
    if (ext == ".fbx" || ext == ".obj") return (ImTextureID)fbx;
    if (ext == ".scene") return (ImTextureID)sceneIcon;
    if (ext == ".hlsl" || ext == ".fx") return (ImTextureID)shaderIcon;
    if (ext == ".cpp" || ext == ".h" || ext == ".cs") return (ImTextureID)scriptIcon;
	if (ext == ".json") return (ImTextureID)JsonIcon;
	if (ext == ".PixAssets") return (ImTextureID)archiveIcon;
	if (ext == ".exe") return (ImTextureID)ExeIcon;
    return (ImTextureID)nullptr;
}

std::string AbbreviateName(const std::string& name, size_t maxLen = 16) {
    size_t dot = name.find_last_of('.');
    std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
    std::string base = (dot != std::string::npos) ? name.substr(0, dot) : name;
    // �g���q�܂߂Ď��܂�ꍇ�͂��̂܂�
    if (base.size() + ext.size() <= maxLen) return name;
    // ���܂�Ȃ��ꍇ�͏ȗ�
    size_t remain = (maxLen > ext.size()) ? maxLen - ext.size() - 3 : 0;
    if (remain > base.size()) remain = base.size();
    return base.substr(0, remain) + "..." + ext;
}

// Visual Studio�Ńt�@�C�����J��
void OpenWithVisualStudio(const std::string& filepath) {
    ShellExecuteA(NULL, "open", "devenv.exe", filepath.c_str(), NULL, SW_SHOWNORMAL);
}

void EditrGUI::ShowContentDrawer() {
    // �����p�X�ݒ�iAssets�t�H���_�j
    if (currentDir.empty()) {
        std::string assetsPath = SettingManager::GetInstance()->GetAssetsFilePath();
        currentDir = assetsPath;
    }

    ImGui::Begin(ShiftJISToUTF8("�R���e���c�h�����[").c_str());

    // �t�H���_�K�w�\���i�߂�{�^���j
    if (currentDir.has_parent_path()) {
        if (ImGui::Button("..")) {
            currentDir = currentDir.parent_path();
        }
        ImGui::SameLine();
        ImGui::Text("%s", currentDir.string().c_str());
    }

    // �g���q�t�B���^�[
    static const char* filterExts[] = { "", ".png", ".jpg", ".obj", ".txt", ".fbx", ".wav", ".mp3", ".ogg", ".hlsl", ".scene", ".cpp", ".h", ".cs" };
    static int filterIndex = 0;
    ImGui::Text(ShiftJISToUTF8("�t�B���^�[:").c_str());
    ImGui::SameLine();
    if (ImGui::Combo("##ExtFilter", &filterIndex, filterExts, IM_ARRAYSIZE(filterExts))) {
        selectedExt = filterExts[filterIndex];
    }

    // �t�H���_�E�t�@�C���ꗗ
    std::vector<std::filesystem::directory_entry> entries;
    for (auto& entry : std::filesystem::directory_iterator(currentDir)) {
        if (entry.is_directory() || selectedExt.empty() || entry.path().extension() == selectedExt) {
            entries.push_back(entry);
        }
    }

    ImGui::BeginChild("assets_grid", ImVec2(0, 0), true);
    float iconSize = 48.0f;
    float itemWidth = 96.0f;
    float itemHeight = 80.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int columns = static_cast<int>(availWidth / itemWidth);
    if (columns < 1) columns = 1;
    ImGui::Columns(columns, nullptr, false);

    static std::filesystem::path selectedEntryPath; // �I�𒆂̃p�X

    for (const auto& entry : entries) {
        ImGui::BeginGroup();

        std::string name = entry.path().filename().string();
        bool isDir = entry.is_directory();
        ImTextureID icon = isDir ? (ImTextureID)folderIcon : GetAssetIcon(this, name);

        float groupX = ImGui::GetCursorPosX();
        float cursorX = groupX + (itemWidth - iconSize) * 0.5f;
        ImGui::SetCursorPosX(cursorX);

        bool isSelected = (entry.path() == selectedEntryPath);

        // ��I�����̓{�^���w�i�F�𓧖���
        if (!isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
        }
        else {
            // �I�����̂ݐn�̐F
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.6f, 1.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.7f, 1.0f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.5f, 1.0f, 1.0f));
        }

        bool iconClicked = false;
        if (icon) {
            iconClicked = ImGui::ImageButton(
                (std::string("icon_") + name).c_str(),
                icon,
                ImVec2(iconSize, iconSize)
            );
        }
        else {
            iconClicked = ImGui::Button(
                (std::string("btn_") + name).c_str(),
                ImVec2(iconSize, iconSize)
            );
        }

        ImGui::PopStyleColor(3); // �K��3��pop����

        float textWidth = ImGui::CalcTextSize(ShiftJISToUTF8(AbbreviateName(name, 12)).c_str()).x;
        ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);
        bool nameClicked = ImGui::Selectable(
            ShiftJISToUTF8(AbbreviateName(name, 12)).c_str(),
            isSelected, 0, ImVec2(itemWidth, 0)
        );

        // �N���b�N����
        if (iconClicked || nameClicked) {
            selectedEntryPath = entry.path();
            if (isDir) {
                currentDir = entry.path();
            }
        }

        ImGui::EndGroup();
        ImGui::NextColumn();
    }
    ImGui::Columns(1);
    ImGui::EndChild();
    ImGui::End();
}