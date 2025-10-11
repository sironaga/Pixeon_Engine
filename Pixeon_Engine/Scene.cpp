#include "Scene.h"
#include "Object.h"
#include "Component.h"
#include "ComponentManager.h"
#include "SettingManager.h"
#include "LightComponent.h"
#include "EngineManager.h"
#include <thread>
#include <mutex>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "System.h"

struct LightGPU {
	DirectX::XMFLOAT3 position; float intensity;
	DirectX::XMFLOAT3 direction; float type;      // type: 0=Dir,1=Point,2=Spot
	DirectX::XMFLOAT3 color;     float range;
	float innerCos; float outerCos; float enabled; float pad; // 16B �A���C�����g
};

static ID3D11Buffer* gLightCB = nullptr;
static const int kMaxLights = 8;

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

	// �J�����R���|�[�l���g�̍X�V
	int i = 0;
	for(auto& obj : _objects){
		if(!obj) continue;
		for(auto& comp : obj->GetComponents()){
			if(!comp) continue;
			if(comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA){
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				cam->SetCameraNumber(i);
				i++;
				comp->EditUpdate();
			}
		}
	}


	if(_MainCamera)_MainCameraNumber = _MainCamera->GetCameraNumber();
	else _MainCameraNumber = -1;


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

	// �J�����R���|�[�l���g�̍X�V
	int i = 0;
	for (auto& obj : _objects) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				cam->SetCameraNumber(i);
				i++;
				comp->EditUpdate();
			}
		}
	}

	if (_MainCamera)_MainCameraNumber = _MainCamera->GetCameraNumber();
	else _MainCameraNumber = -1;

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
	UploadLightsToGPU();
	// �I�u�W�F�N�g�̕`��
	std::vector<Object*> sortedList = _objects;
	if (_MainCamera) {
			std::sort(sortedList.begin(), sortedList.end(), [this](Object* a, Object* b) {
			if (!a || !b) return false;
			// �J��������̋������v�Z
			DirectX::XMFLOAT3 camPos = _MainCamera->GetPosition();
			DirectX::XMFLOAT3 posA = a->GetTransform().position;
			DirectX::XMFLOAT3 posB = b->GetTransform().position;
			float distA = (camPos.x - posA.x) * (camPos.x - posA.x) + (camPos.y - posA.y) * (camPos.y - posA.y) + (camPos.z - posA.z) * (camPos.z - posA.z);
			float distB = (camPos.x - posB.x) * (camPos.x - posB.x) + (camPos.y - posB.y) * (camPos.y - posB.y) + (camPos.z - posB.z) * (camPos.z - posB.z);
			// �������߂����Ƀ\�[�g
			return distA < distB;
					});
	}
	for (auto& obj : sortedList) if (obj)obj->Draw();
}


// �V�[���̕ۑ��@json�`���̏�Ԃ̂܂܊g���q��.scene�ɕύX����
void Scene::SaveToFile(){
	std::vector<Object*> SaveObjects;
	if(EngineManager::GetInstance()->IsInGame()){
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
	SceneData["SceneSettings"]["MainCameraNumber"] = _MainCameraNumber;

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
	_MainCameraNumber = sceneData["SceneSettings"]["MainCameraNumber"].get<int>();

	// Objects�̓ǂݍ���
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = new Object();
		newObj->SetParentScene(this);
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
			else{
				MessageBox(nullptr, "�R���|�[�l���g�̒ǉ��Ɏ��s���܂���", "Error", MB_OK);
			}
		}
		AddObjectLocal(newObj);
	}

	// �o�^����Ă��郁�C���J�����Ɠ����ԍ��̃J�����R���|�[�l���g��T��
	for (auto& obj : _ToBeAdded) {
		if (!obj) continue;
		for (auto& comp : obj->GetComponents()) {
			if (!comp) continue;
			if (comp->GetComponentType() == ComponentManager::COMPONENT_TYPE::CAMERA) {
				CameraComponent* cam = dynamic_cast<CameraComponent*>(comp);
				if (cam->GetCameraNumber() == _MainCameraNumber) {
					SetMainCamera(cam);
					break;
				}
			}
		}
		if (_MainCamera) break;
	}
}

void Scene::RegisterLight(LightComponent* l){
	if (!l) return;
	if (std::find(_lights.begin(), _lights.end(), l) == _lights.end())
		_lights.push_back(l);
}

void Scene::UnregisterLight(LightComponent* l){
	auto it = std::remove(_lights.begin(), _lights.end(), l);
	if (it != _lights.end()) _lights.erase(it, _lights.end());
}

void Scene::ProcessThreadSafeAdditions(){
	std::lock_guard<std::mutex> lock(_mtx);
	for (auto& obj : _ToBeAddedBuffer) {
		if (obj)_ToBeAdded.push_back(obj);
	}
	_ToBeAddedBuffer.clear();
}

void Scene::UploadLightsToGPU(){
	auto dev = DirectX11::GetInstance()->GetDevice();
	auto ctx = DirectX11::GetInstance()->GetContext();
	if (!dev || !ctx) return;

	if (!gLightCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(LightGPU) * kMaxLights + 16; // �]�T
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCB);
	}
	if (!gLightCB) return;

	LightGPU lights[kMaxLights]{};
	int count = 0;
	for (auto* l : _lights) {
		if (!l || !l->IsEnabled()) continue;
		if (count >= kMaxLights) break;
		auto pos = l->GetWorldPosition();
		auto dir = l->GetWorldDirection();
		lights[count].position = pos;
		lights[count].direction = dir;
		lights[count].intensity = l->GetIntensity();
		lights[count].color = l->GetColor();
		lights[count].type = (float)((int)l->GetType());
		lights[count].range = l->GetRange();
		float innerRad = DirectX::XMConvertToRadians(l->GetSpotInner());
		float outerRad = DirectX::XMConvertToRadians(l->GetSpotOuter());
		lights[count].innerCos = cosf(innerRad * 0.5f); // ���p�ň����Ȃ�K�X
		lights[count].outerCos = cosf(outerRad * 0.5f);
		lights[count].enabled = 1.0f;
		++count;
	}

	// ������ LightCount �𖄂߂�� cbuffer �ɕ����Ă��悢������͓��o�b�t�@�����ɏ������� CB �p��
	// �ȑf���̂��� LightCount �p�ǉ� cbuffer
	struct LightCountCB { int count; float pad[3]; };
	static ID3D11Buffer* gLightCountCB = nullptr;
	if (!gLightCountCB) {
		D3D11_BUFFER_DESC bd{};
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(LightCountCB);
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		dev->CreateBuffer(&bd, nullptr, &gLightCountCB);
	}

	// �X�V
	{
		D3D11_MAPPED_SUBRESOURCE mp{};
		if (SUCCEEDED(ctx->Map(gLightCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
			memcpy(mp.pData, lights, sizeof(LightGPU) * kMaxLights);
			ctx->Unmap(gLightCB, 0);
		}
		D3D11_MAPPED_SUBRESOURCE mp2{};
		if (SUCCEEDED(ctx->Map(gLightCountCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp2))) {
			((LightCountCB*)mp2.pData)->count = count;
			ctx->Unmap(gLightCountCB, 0);
		}
	}

	// PS �X�e�[�W�փo�C���h (b1=LightArray, b2=LightCount ��)
	ID3D11Buffer* cbs1[] = { gLightCB };
	ctx->PSSetConstantBuffers(1, 1, cbs1);
	ID3D11Buffer* cbs2[] = { gLightCountCB };
	ctx->PSSetConstantBuffers(2, 1, cbs2);
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



