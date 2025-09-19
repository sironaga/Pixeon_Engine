
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "SettingManager.h"
#include "AssetsManager.h"
#include <filesystem>
#include <vector>
#include <string>

static std::string selectedExt = "";

// ファイル名省略関数（最大maxLen文字＋拡張子は必ず表示）
std::string AbbreviateName(const std::string& name, size_t maxLen = 16) {
    size_t dot = name.find_last_of('.');
    std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
    std::string base = (dot != std::string::npos) ? name.substr(0, dot) : name;
    // 省略する必要がない場合はそのまま
    if (base.size() + ext.size() <= maxLen) return name;
    // 省略する場合
    size_t remain = (maxLen > ext.size()) ? maxLen - ext.size() - 3 : 0;
    if (remain > base.size()) remain = base.size();
    return base.substr(0, remain) + "..." + ext;
}

ImTextureID EditrGUI::GetAssetIcon(EditrGUI* gui, const std::string& name) {
    size_t dot = name.find_last_of('.');
    std::string ext = (dot != std::string::npos) ? name.substr(dot) : "";
    if (ext == ".png" || ext == ".jpg") return (ImTextureID)img;
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return (ImTextureID)Sound;
    if (ext == ".fbx" || ext == ".obj") return (ImTextureID)fbx;
    return (ImTextureID)nullptr;
}

void EditrGUI::ShowContentDrawer() {
    ImGui::Begin(ShiftJISToUTF8("コンテンツドロワー").c_str());

    static const char* filterExts[] = { "", ".png", ".jpg", ".obj", ".txt", ".fbx", ".wav", ".mp3", ".ogg" };
    static int filterIndex = 0;
    ImGui::Text(ShiftJISToUTF8("拡張子フィルター:").c_str());
    ImGui::SameLine();
    if (ImGui::Combo("##ExtFilter", &filterIndex, filterExts, IM_ARRAYSIZE(filterExts))) {
        selectedExt = filterExts[filterIndex];
    }

    auto assets = AssetsManager::GetInstance()->ListAssets();
    if (assets.empty()) {
    }
    else {
        // スクロール可能な範囲
        ImGui::BeginChild("assets_grid", ImVec2(0, 0), true);

        float iconSize = 48.0f;
        float itemWidth = 96.0f;
        float itemHeight = 80.0f;
        float availWidth = ImGui::GetContentRegionAvail().x;
        int columns = static_cast<int>(availWidth / itemWidth);
        if (columns < 1) columns = 1;

        ImGui::Columns(columns, nullptr, false);

        for (const auto& name : assets) {
            ImGui::BeginGroup();

            ImTextureID icon = GetAssetIcon(this, name);
            float groupX = ImGui::GetCursorPosX();
            float cursorX = groupX + (itemWidth - iconSize) * 0.5f;
            ImGui::SetCursorPosX(cursorX);
            if (icon) {
                ImGui::Image(icon, ImVec2(iconSize, iconSize));
            }
            else {
                ImGui::Dummy(ImVec2(iconSize, iconSize));
            }

            std::string shortName = AbbreviateName(name, 12);
            float textWidth = ImGui::CalcTextSize(ShiftJISToUTF8(shortName).c_str()).x;
            ImGui::SetCursorPosX(groupX + (itemWidth - textWidth) * 0.5f);
            ImGui::TextWrapped("%s", ShiftJISToUTF8(shortName).c_str());

            ImGui::EndGroup();

            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ImGui::EndChild();
    }

    ImGui::End();
}