
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
#include "Scene.h"

void EditrGUI::ShowHierarchy()
{
    ImGui::Begin(ShiftJISToUTF8("�q�G�����L�[").c_str());
    // �E�N���b�N�ŃR���e�L�X�g���j���[�\��
    if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight))
    {
        if (ImGui::MenuItem(ShiftJISToUTF8("�I�u�W�F�N�g�̒ǉ�").c_str())) {
            Object* newObj = new Object();
            newObj->SetObjectName("NewObject");
            SceneManger::GetInstance()->GetCurrentScene()->AddObjectLocal(newObj);
        }
        ImGui::EndPopup();
    }

    // �V�[�����̃I�u�W�F�N�g�����X�g�\��
    Scene* currentScene = SceneManger::GetInstance()->GetCurrentScene();
    if (currentScene) {
        std::vector<Object*> objects = currentScene->GetObjects();
        for (size_t i = 0; i < objects.size(); ++i) {
            Object* obj = objects[i];
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (obj == SelectedObject) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)obj, flags, ShiftJISToUTF8(obj->GetObjectName()).c_str());
            if (ImGui::IsItemClicked()) {
                SelectedObject = obj;
            }

            // �I�u�W�F�N�g���ƂɃ��j�[�N�ȃ��x�����쐬
            std::string popupLabel = "ObjectContextMenu_" + std::to_string((intptr_t)obj);

            // �E�N���b�N�ŃR���e�L�X�g���j���[�\��
            if (ImGui::BeginPopupContextItem(popupLabel.c_str(), ImGuiPopupFlags_MouseButtonRight))
            {
                if (ImGui::MenuItem(ShiftJISToUTF8("�폜").c_str())) {
                    SceneManger::GetInstance()->GetCurrentScene()->RemoveObject(obj);
                    SelectedObject = nullptr;
                }
                ImGui::EndPopup();
            }
            if (nodeOpen) {
                ImGui::TreePop();
            }
        }
    }
    ImGui::End();
}
