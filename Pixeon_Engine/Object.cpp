#include "Object.h"
#include "Component.h"

void Object::Init(){

}

void Object::BeginPlay(){
	for(auto comp : _components)if(comp)comp->BeginPlay();
}

void Object::EditUpdate(){
	for(auto comp : _components)if(comp)comp->EditUpdate();
}

void Object::InGameUpdate(){
	for(auto comp : _components)if(comp)comp->InGameUpdate();
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
