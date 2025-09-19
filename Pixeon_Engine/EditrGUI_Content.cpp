
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "SettingManager.h"
#include "AssetsManager.h"

void EditrGUI::ShowContentDrawer()
{
    ImGui::Begin(ShiftJISToUTF8("�R���e���c�h�����[").c_str());

    ImGui::Text(ShiftJISToUTF8("�A�Z�b�g�ꗗ").c_str());
    auto assets = AssetsManager::GetInstance()->ListAssets();

    if (assets.empty()) {
        ImGui::Text(ShiftJISToUTF8("�A�Z�b�g��������܂���B").c_str());
    }
    else {
        ImGui::BeginChild("assets_list", ImVec2(0, 300), true);
        for (const auto& name : assets) {
            ImGui::Selectable(ShiftJISToUTF8(name).c_str());
        }
        ImGui::EndChild();
    }

    ImGui::End();
}