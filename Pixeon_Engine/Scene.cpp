#include "Scene.h"
#include "Object.h"
#include "Main.h"
#include "Component.h"
#include "ComponentManager.h"
#include "SettingManager.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

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

void Scene::Draw() {

	// �I�u�W�F�N�g�̕`��
	std::vector<Object*> sortedList = _objects;
	if (true) {
	}
}


// �V�[���̕ۑ��@json�`���̏�Ԃ̂܂܊g���q��.scene�ɕύX����
void Scene::SaveToFile(){
	MessageBox(nullptr, "�V�[����ۑ����܂����H", "�V�[���ۑ�", MB_OKCANCEL);
	std::vector<Object*> SaveObjects;
	if(IsInGame()){
		SaveObjects = _SaveObjects;
	}
	else{
		SaveObjects = _objects;
	}
	// ���ݎ����̎擾
	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = _name;

	// �I�u�W�F�N�g�f�[�^�̕ۑ�
	nlohmann::json ObjectArray	= nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			// �I�u�W�F�N�g�̊�{���̕ۑ�
			nlohmann::json ObjectData;
			ObjectData["Name"] = Object->GetObjectName();
			ObjectData["Transform"]["Position"] = { Object->GetTransform().position.x, Object->GetTransform().position.y, Object->GetTransform().position.z };
			ObjectData["Transform"]["Rotation"] = { Object->GetTransform().rotation.x, Object->GetTransform().rotation.y, Object->GetTransform().rotation.z };
			ObjectData["Transform"]["Scale"] = { Object->GetTransform().scale.x,    Object->GetTransform().scale.y,    Object->GetTransform().scale.z };

			// �R���|�[�l���g�f�[�^�̕ۑ�
			nlohmann::json ComponentData = nlohmann::json::array();
			for (const auto& comp : Object->GetComponents()) {
				if (comp) {
					nlohmann::json CompJson;
					CompJson["Type"] = comp->GetComponentType();
					CompJson["Name"] = comp->GetComponentName();
					std::ostringstream oss;
					comp->SaveToFile(oss);
					CompJson["Data"] = oss.str();
					ComponentData.push_back(CompJson);
				}
			}
			ObjectData["Components"] = ComponentData;
			ObjectArray.push_back(ObjectData);
		}
	}
	SceneData["Objects"] = ObjectArray;

	// �t�@�C�����̐���
	std::string File;
	File = SettingManager::GetInstance()->GetSceneFilePath() + _name + ".scene";
	std::ofstream outFile(File);
	if(outFile.is_open()) {
		outFile << SceneData.dump(4); // �C���f���g��4�ŕۑ�
		outFile.close();
	}
}

void Scene::LoadToFile(){
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + _name + ".scene";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		// �t�@�C�����J���Ȃ������ꍇ�Afalse��Ԃ�
		return ;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	_name = sceneData["SceneSettings"]["Name"].get<std::string>();

	// Objects�̓ǂݍ���
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = new Object();
		newObj->SetObjectName(objData["Name"].get<std::string>());
		// Transform�̓ǂݍ���
		auto pos = objData["Transform"]["Position"];
		auto rot = objData["Transform"]["Rotation"];
		auto scl = objData["Transform"]["Scale"];
		Transform transform;
		transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
		transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
		transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
		newObj->SetTransform(transform);
		// �R���|�[�l���g�̓ǂݍ���
		for (const auto& compData : objData["Components"]) {
			auto type = static_cast<ComponentManager::COMPONENT_TYPE>(compData["Type"].get<int>());
			auto name = compData["Name"].get<std::string>();
			auto data = compData["Data"].get<std::string>();
			Component* newComp = ComponentManager::GetInstance()->AddComponent(newObj,type);
			if (newComp) {
				newComp->SetComponentName(name);
				std::istringstream iss(data);
				newComp->LoadFromFile(iss);
			}
		}
		AddObjectLocal(newObj);
	}
}

void Scene::ProcessThreadSafeAdditions(){
	std::lock_guard<std::mutex> lock(_mtx);
	for (auto& obj : _ToBeAddedBuffer) {
		if (obj)_ToBeAdded.push_back(obj);
	}
	_ToBeAddedBuffer.clear();
}

// ���[�J���X���b�h�ł̃I�u�W�F�N�g�ǉ�
void Scene::AddObjectLocal(Object* obj){
	if (obj) {
		_ToBeAdded.push_back(obj);
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



