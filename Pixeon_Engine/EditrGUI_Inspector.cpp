
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imgui_impl_dx11.h"
#include "IMGUI/imgui_impl_win32.h"
#include "IMGUI/imgui_internal.h"
#include "System.h"
#include "Main.h"
#include "StartUp.h"
#include "SettingManager.h"
#include "SceneManger.h"
#include "Object.h"
#include "Component.h"

void EditrGUI::ShowInspector()
{
    ImGui::Begin(ShiftJISToUTF8("インスペクター").c_str());
    if (SelectedObject){
		ImGui::Text(ShiftJISToUTF8("オブジェクト名: %s").c_str(), SelectedObject->GetObjectName().c_str());
		ImGui::Separator();
		if (ImGui::CollapsingHeader(ShiftJISToUTF8("Transform").c_str())) {
			Transform TempTransform = SelectedObject->GetTransform();
			if (ImGui::DragFloat3(ShiftJISToUTF8("位置").c_str(), &TempTransform.position.x, 0.1f)) {
				SelectedObject->SetPosition(TempTransform.position.x, TempTransform.position.y, TempTransform.position.z);
			}
			if (ImGui::DragFloat3(ShiftJISToUTF8("回転").c_str(), &TempTransform.rotation.x, 0.1f)) {
				SelectedObject->SetRotation(TempTransform.rotation.x, TempTransform.rotation.y, TempTransform.rotation.z);
			}
			if (ImGui::DragFloat3(ShiftJISToUTF8("スケール").c_str(), &TempTransform.scale.x, 0.1f)) {
				SelectedObject->SetScale(TempTransform.scale.x, TempTransform.scale.y, TempTransform.scale.z);
			}
		}
		ImGui::Separator();
		for (auto& comp : SelectedObject->GetComponents()) {
			if (comp) {
				if (ImGui::CollapsingHeader(ShiftJISToUTF8(comp->GetComponentName().c_str()).c_str())) {
					comp->DrawInspector();
					ImGui::Separator();
				}
			}
		}
    }
    ImGui::End();
}