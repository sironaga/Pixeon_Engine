
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"

void EditrGUI::ShowHierarchy()
{
    ImGui::Begin(ShiftJISToUTF8("ヒエラルキー").c_str());
    ImGui::Text(ShiftJISToUTF8("ここにシーンのオブジェクト階層を表示します。").c_str());
    ImGui::End();
}
