
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
#include "ComponentManager.h"
void EditrGUI::ShowInspector()
{
    ImGui::Begin(ShiftJISToUTF8("インスペクター").c_str());
    if (SelectedObject){
		ImGui::Text(ShiftJISToUTF8("オブジェクト名: %s").c_str(), SelectedObject->GetObjectName().c_str());
		ImGui::Separator();
		ImGui::BeginChild("InspectorChild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
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
			if (comp) 
			{
				comp->DrawInspector();
			}
		}


		//　コンポーネント追加UI
		std::string ComponentList[(int)ComponentManager::COMPONENT_TYPE::MAX];
		for (int i = 0; i < (int)ComponentManager::COMPONENT_TYPE::MAX; i++) {
			ComponentList[i] = ComponentManager::GetInstance()->GetComponentName((ComponentManager::COMPONENT_TYPE)i);
		}
		static int CurrentComponent = 0;
		ImGui::Combo(ShiftJISToUTF8("コンポーネント追加").c_str(), &CurrentComponent, [](void* data, int idx, const char** out_text) {
			std::string* items = (std::string*)data;
			if (out_text) { *out_text = items[idx].c_str(); }
			return true;
			}, ComponentList, IM_ARRAYSIZE(ComponentList), IM_ARRAYSIZE(ComponentList));
		ImGui::SameLine();
		if (ImGui::Button(ShiftJISToUTF8("追加").c_str())) ComponentManager::GetInstance()->AddComponent(SelectedObject, (ComponentManager::COMPONENT_TYPE)CurrentComponent);

		ImGui::EndChild();

    }
    ImGui::End();
}