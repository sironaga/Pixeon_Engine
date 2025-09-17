#include "ComponentManager.h"
#include "Component.h"
#include <Windows.h>

Component* ComponentManager::AddComponent(Object* owner, COMPONENT_TYPE type){

	if (!owner) return nullptr;

	Component* component = nullptr;

	switch (type)
	{
	case ComponentManager::COMPONENT_TYPE::NONE:
		MessageBox(nullptr, "��O�Ȓl�ł�\nCode : CMMIN", "Error", MB_OK);
		break;
	case ComponentManager::COMPONENT_TYPE::MAX:
		MessageBox(nullptr, "��O�Ȓl�ł�\nCode : CMMAX", "Error", MB_OK);
		break;
	default:
		MessageBox(nullptr, "�o�^����Ă��܂���\nCode CMFIND", "Error", MB_OK);
		break;
	}
  
}
