#include "Object.h"
#include "Component.h"

void Object::Init(){

}

void Object::BeginPlay(){
	_TempTransform = _transform;
	for(auto comp : _components)comp->BeginPlay();
}

void Object::EditUpdate(){
	if (_isInGame) {
		_transform = _TempTransform;
		_isInGame = false;
	}
	for(auto comp : _components)comp->EditUpdate();
}

void Object::InGameUpdate(){
	_isInGame = true;
	for(auto comp : _components)comp->InGameUpdate();
}

void Object::Draw(){
	for(auto comp : _components)comp->Draw();
}

void Object::UInit(){
	for (auto comp : _components) {
		comp->UInit();
		delete comp;
	}
	_components.clear();
}

Component* Object::GetComponent(const std::string& name)
{
	for(auto comp : _components) {
		if (comp->GetComponentName() == name) {
			return comp;
		}
	}
	return nullptr;
}

void Object::RemoveComponent(Component* comp){
	if (comp == nullptr) return;
	auto it = std::remove(_components.begin(), _components.end(), comp);
	if(it != _components.end()) {
		_components.erase(it, _components.end());
		comp->UInit();
		delete comp;
	}
}
