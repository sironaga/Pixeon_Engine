#pragma once
// �R���|�[�l���g�}�l�[�W���[�N���X
// �R���|�[�l���g�̊Ǘ����s��

#include <string>
#include <vector>
#include "Object.h"

class Component;

class ComponentManager
{
public:
	enum class COMPONENT_TYPE {
		NONE = -1,
		CAMERA,
		MAX,
	};

public:
	static ComponentManager* GetInstance();
	static void DestroyInstance();
	void Init();
public:
	Component* AddComponent(Object* owner, COMPONENT_TYPE type);
	std::string GetComponentName(COMPONENT_TYPE type) { return _ComponentName[(int)type]; }
private:
	std::string _ComponentName[(int)COMPONENT_TYPE::MAX];
private:
	static ComponentManager* _instance;
private:
	ComponentManager() {}
	~ComponentManager() {}
};

