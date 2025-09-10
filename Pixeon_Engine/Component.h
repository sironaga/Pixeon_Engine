#pragma once

#include <string>
#include <fstream>
#include <sstream>

class Object;

class Component
{
public:
	enum class ComponetKinds
	{
		NONE,
	};
public:
	virtual void Init(Object* Prt)	{}
	virtual void BeginPlay()		{}
	virtual void EditUpdate()		{}
	virtual void InGameUpdate()		{}
	virtual void Draw()				{}
	virtual void UInit()			{}

public:
	virtual void SaveToFile(std::ostream& out) {}
	virtual void LoadFromFile(std::istream& in){}

	ComponetKinds GetComponentKind()			{ return _Kind; }
	void SetComponentKind(ComponetKinds Kind)	{ _Kind = Kind; }

	Object* GetParent() const { return _Parent; }

	std::string GetComponentName() const { return _ComponentName; }
	void SetComponentName(const std::string& name) { _ComponentName = name; }

protected:
	std::string _ComponentName;
	Object* _Parent;
	ComponetKinds _Kind;
};

