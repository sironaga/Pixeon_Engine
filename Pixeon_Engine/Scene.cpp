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
#include <filesystem>

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
	for (auto& obj : _ToBeRemoved) {
		if (!obj) continue;
		auto it = std::find(_objects.begin(), _objects.end(), obj);
		if (it != _objects.end()) {
			_objects.erase(it);
			delete obj;
		}
	}
	_ToBeRemoved.clear();
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
	for (auto& obj : _ToBeRemoved) {
		if (!obj) continue;
		auto it = std::find(_objects.begin(), _objects.end(), obj);
		if (it != _objects.end()) {
			_objects.erase(it);
			delete obj;
		}
	}
	_ToBeRemoved.clear();
}

void Scene::Draw() {

	// �I�u�W�F�N�g�̕`��
	std::vector<Object*> sortedList = _objects;
	if (true) {
	}
}


// �V�[���̕ۑ��@json�`���̏�Ԃ̂܂܊g���q��.scene�ɕύX����
void Scene::SaveToFile(){
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

	// �t�@�C�����̐��� - �p�X���؂����������v��
	std::string File;
	std::string sceneDir = SettingManager::GetInstance()->GetSceneFilePath();
	// �p�X���؂��������������ꍇ�͒ǉ��Ȃ�
	if (!sceneDir.empty() && sceneDir.back() != '/' && sceneDir.back() != '\\') {
		sceneDir += "/";
	}
	File = sceneDir + _name + ".scene";
	
	try {
		// �ڃf�B���N�g���̕\�ڋ���
		std::filesystem::path scenePath(File);
		std::filesystem::path sceneParentDir = scenePath.parent_path();
		
		if (!std::filesystem::exists(sceneParentDir)) {
			try {
				std::filesystem::create_directories(sceneParentDir);
			}
			catch (const std::filesystem::filesystem_error& e) {
				MessageBox(nullptr, ("�V�[���f�B���N�g���̍쐬�Ɏ��s: " + std::string(e.what())).c_str(), "�Z�[�u�G���[", MB_OK | MB_ICONERROR);
				return;
			}
		}
		
		std::ofstream outFile(File);
		if(outFile.is_open()) {
			outFile << SceneData.dump(4); // �C���f���g��4�ŕۑ�
			if (outFile.fail()) {
				// ����������s�̃G���[�n���h�����O
				MessageBox(nullptr, ("�V�[���t�@�C���̏�������s���܂���: " + File).c_str(), "�Z�[�u�G���[", MB_OK | MB_ICONERROR);
			}
			outFile.close();
		} else {
			// �t�@�C���I�[�v���G���[
			MessageBox(nullptr, ("�V�[���t�@�C���J���܂���: " + File).c_str(), "�Z�[�u�G���[", MB_OK | MB_ICONERROR);
		}
	}
	catch (const nlohmann::json::exception& e) {
		// JSON�V���A���C�Y�G���[
		MessageBox(nullptr, ("JSON�V���A���C�Y�G���[: " + std::string(e.what())).c_str(), "�Z�[�u�G���[", MB_OK | MB_ICONERROR);
	}
	catch (const std::exception& e) {
		// ���̑��̃G���[
		MessageBox(nullptr, ("�V�[���Z�[�u���G���[: " + std::string(e.what())).c_str(), "�Z�[�u�G���[", MB_OK | MB_ICONERROR);
	}
}

void Scene::LoadToFile(){
	// �p�X����̓v�� - SaveToFile�Ɠ���p�X�\�z��g�p
	std::string sceneDir = SettingManager::GetInstance()->GetSceneFilePath();
	// �p�X���؂��������������ꍇ�͒ǉ��Ȃ�
	if (!sceneDir.empty() && sceneDir.back() != '/' && sceneDir.back() != '\\') {
		sceneDir += "/";
	}
	std::string filePath = sceneDir + _name + ".scene";
	
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		// �t�@�C�����J���Ȃ������ꍇ�Afalse��Ԃ�
		return ;
	}

	try {
		nlohmann::json sceneData;
		inFile >> sceneData;
		inFile.close();

		// JSON�X�L�[�}�̑��݂`�F�b�N
		if (!sceneData.contains("SceneSettings") || !sceneData["SceneSettings"].contains("Name")) {
			MessageBox(nullptr, ("�V�[���t�@�C���̃t�H�[�}�b�g���s���ł�: " + filePath).c_str(), "���[�h�G���[", MB_OK | MB_ICONERROR);
			return;
		}
		if (!sceneData.contains("Objects") || !sceneData["Objects"].is_array()) {
			MessageBox(nullptr, ("�V�[���t�@�C���ɃI�u�W�F�N�g�f�[�^��������܂���: " + filePath).c_str(), "���[�h�G���[", MB_OK | MB_ICONERROR);
			return;
		}

		_name = sceneData["SceneSettings"]["Name"].get<std::string>();

	// Objects�̓ǂݍ���
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = nullptr;
		try {
			newObj = new Object();
			
			// �I�u�W�F�N�g���̌�����
			if (objData.contains("Name")) {
				newObj->SetObjectName(objData["Name"].get<std::string>());
			}
			
			// Transform�̓ǂݍ���
			if (objData.contains("Transform")) {
				const auto& transformData = objData["Transform"];
				Transform transform;
				
				if (transformData.contains("Position") && transformData["Position"].is_array() && transformData["Position"].size() == 3) {
					auto pos = transformData["Position"];
					transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
				}
				if (transformData.contains("Rotation") && transformData["Rotation"].is_array() && transformData["Rotation"].size() == 3) {
					auto rot = transformData["Rotation"];
					transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
				}
				if (transformData.contains("Scale") && transformData["Scale"].is_array() && transformData["Scale"].size() == 3) {
					auto scl = transformData["Scale"];
					transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
				}
				newObj->SetTransform(transform);
			}
			
			// �R���|�[�l���g�̓ǂݍ���
			if (objData.contains("Components") && objData["Components"].is_array()) {
				for (const auto& compData : objData["Components"]) {
					if (compData.contains("Type") && compData.contains("Name") && compData.contains("Data")) {
						auto type = static_cast<ComponentManager::COMPONENT_TYPE>(compData["Type"].get<int>());
						auto name = compData["Name"].get<std::string>();
						auto data = compData["Data"].get<std::string>();
						Component* newComp = ComponentManager::GetInstance()->AddComponent(newObj, type);
						if (newComp) {
							newComp->SetComponentName(name);
							try {
								std::istringstream iss(data);
								newComp->LoadFromFile(iss);
							}
							catch (const std::exception& e) {
								// �R���|�[�l���g���[�h�G���[�͌x���Ƃ��Ĉ���
								MessageBox(nullptr, ("�R���|�[�l���g���[�h�G���[: " + std::string(e.what())).c_str(), "���[�h�x��", MB_OK | MB_ICONWARNING);
							}
						}
					}
				}
			}
			
			AddObjectLocal(newObj);
			newObj = nullptr; // ����ɒǉ����ꂽ�̂ŏ��L����null��ݒ�
		}
		catch (const nlohmann::json::exception& e) {
			// JSON�p�[�X�G���[
			if (newObj) {
				delete newObj; // ���������������I�u�W�F�N�g���폜
			}
			MessageBox(nullptr, ("�I�u�W�F�N�g�f�[�^�̉���G���[: " + std::string(e.what())).c_str(), "���[�h�G���[", MB_OK | MB_ICONWARNING);
			continue; // ���̃I�u�W�F�N�g�̏����ɑ���
		}
		catch (const std::exception& e) {
			// ���̑��̃G���[
			if (newObj) {
				delete newObj;
			}
			MessageBox(nullptr, ("�I�u�W�F�N�g�쐬�G���[: " + std::string(e.what())).c_str(), "���[�h�G���[", MB_OK | MB_ICONWARNING);
			continue;
		}
	}
	}
	catch (const nlohmann::json::exception& e) {
		// JSON�p�[�X�G���[
		MessageBox(nullptr, ("JSON�p�[�X�G���[: " + std::string(e.what())).c_str(), "���[�h�G���[", MB_OK | MB_ICONERROR);
		return;
	}
	catch (const std::exception& e) {
		// ���̑��̃G���[
		MessageBox(nullptr, ("�V�[�����[�h�G���[: " + std::string(e.what())).c_str(), "���[�h�G���[", MB_OK | MB_ICONERROR);
		return;
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

void Scene::RemoveObject(Object* obj){
	if (!obj) return;
	_ToBeRemoved.push_back(obj);
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



