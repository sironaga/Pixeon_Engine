//	�R���|�[�l���g�̍X�V�A�`����Ǘ�����N���X

#pragma once

#include "Struct.h"
#include <string>
#include <vector>
#include <map>

class Component;
class Scene;

class Object
{
public:
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void InGameUpdate();
	virtual void Draw();
	virtual void UInit();

	Object* Clone();

public:

	// Setter And Getter
	Transform GetTransform() { return _transform; }
	void SetTransform(Transform transform) { _transform = transform; }

	std::string GetObjectName() { return _ObjectName; }
	void SetObjectName(const std::string& name) { _ObjectName = name; }

	void SetPosition(float x, float y, float z) {
		_transform.position = { x, y, z };
	}

	void SetRotation(float x, float y, float z) {
		_transform.rotation = { x, y, z };
	}

	void SetScale(float x, float y, float z) {
		_transform.scale = { x, y, z };
	}

public:
	// �R���|�[�l���g�̒ǉ�
	template<typename T = Component>
	T* AddComponent() {
		T* newComp = new T();
		newComp->Init(this);
		_components.push_back(newComp);
		return newComp;
	}
	// ���O����R���|�[�l���g���擾
	Component* GetComponent(const std::string& name);
	// �^����R���|�[�l���g���擾
	template<typename T = Component>
	T* GetComponent() {
		for (auto comp : _components) {
			T* castedComp = dynamic_cast<T*>(comp);
			if (castedComp) {
				return castedComp;
			}
		}
		return nullptr;
	}
	// �R���|�[�l���g�̍폜
	void RemoveComponent(Component* comp);

	std::vector<Component*> GetComponents() { return _components; }

	void SetParentScene(Scene* scene) { _ParentScene = scene; }
	Scene* GetParentScene() const { return _ParentScene; }

public:
	// variable Setter And Getter
	void SetInt(const std::string& key, int value) { _intValues[key] = value; }
	void SetFloat(const std::string& key, float value) { _floatValues[key] = value; }
	void SetBool(const std::string& key, bool value) { _boolValues[key] = value; }
	int GetInt(const std::string& key) { return _intValues[key]; }
	float GetFloat(const std::string& key) { return _floatValues[key]; }
	bool GetBool(const std::string& key) { return _boolValues[key]; }

protected:
	std::string _ObjectName;
	Transform _transform;

	std::vector<Component*> _components;

	Scene* _ParentScene = nullptr;

	std::map<std::string, int>		_intValues;
	std::map<std::string, float>	_floatValues;
	std::map<std::string, bool>		_boolValues;
};



