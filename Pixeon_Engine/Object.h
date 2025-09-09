#pragma once

#include "Struct.h"
#include <string>
#include <vector>
#include <map>

class Component;

class Object
{
public:
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditerUpdate();
	virtual void InGameUpdate();
	virtual void Draw();
	virtual void UInit();

	Transform GetTransform() { return _transform; }
	void SetTransform(Transform transform) { _transform = transform; }

	std::string GetObjectName() { return _ObjectName; }
	void SetObjectName(const std::string& name) { _ObjectName = name; }

public:


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


	std::map<std::string, int>	_intValues;
	std::map<std::string, float>	_floatValues;
	std::map<std::string, bool>		_boolValues;
};

