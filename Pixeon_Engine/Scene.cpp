#include "Scene.h"
#include "Object.h"
#include <thread>
#include <mutex>

// �J������
Scene::~Scene(){
}

void Scene::Init(){
	// ���I�z��̏�����
	_objects.clear();
	_ToBeAdded.clear();
	_ToBeRemoved.clear();
	_SaveObjects.clear();

}

void Scene::BeginPlay(){
	for (auto& obj : _objects) {
		if (obj) {
			Object* cloneObj = obj->Clone();
			_SaveObjects.push_back(cloneObj);
		}
	}

	// �������Z�Ɋւ���R�[�h
	
	//

	// BeginPlay���Ăяo��
	for (auto& obj : _objects) {
		if (obj)obj->BeginPlay();
	}
}

void Scene::EditUpdate(){
	// �񓯊��ǉ��̏���
	ProcessThreadSafeAdditions();
	// �I�u�W�F�N�g�̒ǉ�����
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// �J�����̍X�V

	//

	// �I�u�W�F�N�g�̍X�V
	for (auto& obj : _objects) if (obj)obj->EditUpdate();

	// �I�u�W�F�N�g�̍폜����

}

void Scene::PlayUpdate(){
	// �񓯊��ǉ��̏���
	ProcessThreadSafeAdditions();
	// �I�u�W�F�N�g�̒ǉ�����
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			obj->BeginPlay();
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// �J�����̍X�V

	//

	// �I�u�W�F�N�g�̍X�V
	for (auto& obj : _objects) if (obj)obj->InGameUpdate();

	// �I�u�W�F�N�g�̍폜����
}

void Scene::Draw(){

	// �I�u�W�F�N�g�̕`��
	std::vector<Object*> sortedList = _objects;
	if (true) {
	}
}

// �I�u�W�F�N�g�̔񓯊��ǉ�
bool Scene::AddObject(Object* obj)
{
	if (!obj) return false;
	std::thread([this, obj]() {
		Object* newObj = obj->Clone();
		if (newObj) {
			std::lock_guard<std::mutex>lock(_mtx);
			_ToBeAddedBuffer.push_back(newObj);
		}
		}).detach();
	return true;
}

void Scene::SaveToFile(){
}

void Scene::LoadToFile(){
}

void Scene::ProcessThreadSafeAdditions(){
	std::lock_guard<std::mutex> lock(_mtx);
	for (auto& obj : _ToBeAddedBuffer) {
		if (obj)_ToBeAdded.push_back(obj);
	}
	_ToBeAddedBuffer.clear();
}



