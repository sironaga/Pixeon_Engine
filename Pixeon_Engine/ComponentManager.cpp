#include "ComponentManager.h"
#include "Component.h"
#include <Windows.h>

Component* ComponentManager::AddComponent(Object* owner, COMPONENT_TYPE type){

	if (!owner) return nullptr;

	Component* component = nullptr;

	switch (type)
	{
	case ComponentManager::COMPONENT_TYPE::NONE:
		MessageBox(nullptr, "—áŠO‚È’l‚Å‚·\nCode : CMMIN", "Error", MB_OK);
		break;
	case ComponentManager::COMPONENT_TYPE::MAX:
		MessageBox(nullptr, "—áŠO‚È’l‚Å‚·\nCode : CMMAX", "Error", MB_OK);
		break;
	default:
		MessageBox(nullptr, "“o˜^‚³‚ê‚Ä‚¢‚Ü‚¹‚ñ\nCode CMFIND", "Error", MB_OK);
		break;
	}
  
}
