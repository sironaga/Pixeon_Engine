
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"

void EditrGUI::ShowInspector()
{
    ImGui::Begin(ShiftJISToUTF8("�C���X�y�N�^�[").c_str());
    ImGui::Text(ShiftJISToUTF8("�����ɑI���I�u�W�F�N�g�̏ڍ׏���\�����܂��B").c_str());
    ImGui::End();
}