#pragma once

#include "Struct.h"
#include <string>
#include <vector>
#include <map>

class Component;

class Object
{





public:
	// variable Setter And Getter
	void SetInt(const std::string& key, int value) { intValues[key] = value; }
	void SetFloat(const std::string& key, float value) { floatValues[key] = value; }
	void SetBool(const std::string& key, bool value) { boolValues[key] = value; }
	int GetInt(const std::string& key) { return intValues[key]; }
	float GetFloat(const std::string& key) { return floatValues[key]; }
	bool GetBool(const std::string& key) { return boolValues[key]; }

protected:
	Transform _transform;
	std::vector<Component*> _components;



	std::map<std::string, int> intValues;
	std::map<std::string, float> floatValues;
	std::map<std::string, bool> boolValues;
};

