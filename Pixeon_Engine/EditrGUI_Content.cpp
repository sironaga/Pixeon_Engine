
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
    ImGui::Begin(ShiftJISToUTF8("�R���e���c�h�����[").c_str());
    ImGui::Text(ShiftJISToUTF8("�����ɃA�Z�b�g�ꗗ��\�����܂��B").c_str());
    ImGui::End();
}