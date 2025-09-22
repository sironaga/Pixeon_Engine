#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include "ComponentManager.h"

class Object;

class Component
{
public:
	virtual void Init(Object* Prt)	{}
	virtual void BeginPlay()		{}
	virtual void EditUpdate()		{}
	virtual void InGameUpdate()		{}
	virtual void Draw()				{}
	virtual void UInit()			{}
	virtual void DrawInspector()	{}


public:
	virtual void SaveToFile(std::ostream& out) {}
	virtual void LoadFromFile(std::istream& in){}

	Object* GetParent() const { return _Parent; }

	std::string GetComponentName() const { return _ComponentName; }
	void SetComponentName(const std::string& name) { _ComponentName = name; }

	ComponentManager::COMPONENT_TYPE GetComponentType() const { return _Type; }
	void SetComponentType(ComponentManager::COMPONENT_TYPE type) { _Type = type; }



protected:
	std::string _ComponentName;
	Object* _Parent;
	ComponentManager::COMPONENT_TYPE _Type = ComponentManager::COMPONENT_TYPE::NONE;
};

