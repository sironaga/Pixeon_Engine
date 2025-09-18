
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"

void EditrGUI::ShowContentDrawer()
{
    ImGui::Begin(ShiftJISToUTF8("コンテンツドロワー").c_str());
    ImGui::Text(ShiftJISToUTF8("ここにアセット一覧を表示します。").c_str());
    ImGui::End();
}