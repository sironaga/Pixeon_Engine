#include "ComponentManager.h"
#include "Component.h"
#include "CameraComponent.h"
#include <Windows.h>

ComponentManager* ComponentManager::_instance;

ComponentManager* ComponentManager::GetInstance(){
	if (_instance == nullptr) {
		_instance = new ComponentManager();
	}
	return _instance;
}

void ComponentManager::DestroyInstance(){
	if (_instance) {
		delete _instance;
		_instance = nullptr;
	}
}

void ComponentManager::Init(){
	_ComponentName[(int)COMPONENT_TYPE::CAMERA] = "Camera";
}

Component* ComponentManager::AddComponent(Object* owner, COMPONENT_TYPE type){

	if (!owner) return nullptr;

	Component* component = nullptr;

	switch (type)
	{
	case ComponentManager::COMPONENT_TYPE::NONE:

		break;
	case ComponentManager::COMPONENT_TYPE::CAMERA:
		component = owner->AddComponent<CameraComponent>();
		break;
	case ComponentManager::COMPONENT_TYPE::MAX:
		MessageBox(nullptr, "��O�Ȓl�ł�\nCode : CMMAX", "Error", MB_OK);
		break;
	default:
		MessageBox(nullptr, "�o�^����Ă��܂���\nCode CMFIND", "Error", MB_OK);
		break;
	}
	return component;
}
