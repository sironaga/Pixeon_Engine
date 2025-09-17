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

// 開放処理
Scene::~Scene(){
}

void Scene::Init(){
	// 動的配列の初期化
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

	// 物理演算に関するコード
	
	//

	// BeginPlayを呼び出す
	for (auto& obj : _objects) {
		if (obj)obj->BeginPlay();
	}
}

void Scene::EditUpdate(){
	// 非同期追加の処理
	ProcessThreadSafeAdditions();
	// オブジェクトの追加処理
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// カメラの更新

	//

	// オブジェクトの更新
	for (auto& obj : _objects) if (obj)obj->EditUpdate();
	
	// オブジェクトの削除処理

}

void Scene::PlayUpdate(){
	// 非同期追加の処理
	ProcessThreadSafeAdditions();
	// オブジェクトの追加処理
	for (auto& obj : _ToBeAdded) {
		if (obj) {
			obj->SetParentScene(this);
			obj->BeginPlay();
			_objects.push_back(obj);
		}
	}
	_ToBeAdded.clear();

	// カメラの更新

	//

	// オブジェクトの更新
	for (auto& obj : _objects) if (obj)obj->InGameUpdate();

	// オブジェクトの削除処理
}

void Scene::Draw() {

	// オブジェクトの描画
	std::vector<Object*> sortedList = _objects;
	if (true) {
	}
}


// シーンの保存　json形式の状態のまま拡張子を.sceneに変更する
void Scene::SaveToFile(){
	MessageBox(nullptr, "シーンを保存しますか？", "シーン保存", MB_OKCANCEL);
	std::vector<Object*> SaveObjects;
	if(IsInGame()){
		SaveObjects = _SaveObjects;
	}
	else{
		SaveObjects = _objects;
	}
	// 現在時刻の取得
	auto Now = std::chrono::system_clock::now();
	auto in_time_t = std::chrono::system_clock::to_time_t(Now);
	std::tm localtime;
	localtime_s(&localtime, &in_time_t);

	nlohmann::json SceneData;
	SceneData["SceneSettings"]["Name"] = _name;

	// オブジェクトデータの保存
	nlohmann::json ObjectArray	= nlohmann::json::array();

	for (const auto& Object : SaveObjects) {
		if (Object) {
			// オブジェクトの基本情報の保存
			nlohmann::json ObjectData;
			ObjectData["Name"] = Object->GetObjectName();
			ObjectData["Transform"]["Position"] = { Object->GetTransform().position.x, Object->GetTransform().position.y, Object->GetTransform().position.z };
			ObjectData["Transform"]["Rotation"] = { Object->GetTransform().rotation.x, Object->GetTransform().rotation.y, Object->GetTransform().rotation.z };
			ObjectData["Transform"]["Scale"] = { Object->GetTransform().scale.x,    Object->GetTransform().scale.y,    Object->GetTransform().scale.z };

			// コンポーネントデータの保存
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

	// ファイル名の生成
	std::string File;
	File = SettingManager::GetInstance()->GetSceneFilePath() + _name + ".scene";
	std::ofstream outFile(File);
	if(outFile.is_open()) {
		outFile << SceneData.dump(4); // インデント幅4で保存
		outFile.close();
	}
}

void Scene::LoadToFile(){
	std::string filePath = SettingManager::GetInstance()->GetSceneFilePath() + "/" + _name + ".scene";
	std::ifstream inFile(filePath);
	if (!inFile.is_open()) {
		// ファイルが開けなかった場合、falseを返す
		return ;
	}

	nlohmann::json sceneData;
	inFile >> sceneData;
	inFile.close();

	_name = sceneData["SceneSettings"]["Name"].get<std::string>();

	// Objectsの読み込み
	for (const auto& objData : sceneData["Objects"]) {
		Object* newObj = new Object();
		newObj->SetObjectName(objData["Name"].get<std::string>());
		// Transformの読み込み
		auto pos = objData["Transform"]["Position"];
		auto rot = objData["Transform"]["Rotation"];
		auto scl = objData["Transform"]["Scale"];
		Transform transform;
		transform.position = { pos[0].get<float>(), pos[1].get<float>(), pos[2].get<float>() };
		transform.rotation = { rot[0].get<float>(), rot[1].get<float>(), rot[2].get<float>() };
		transform.scale = { scl[0].get<float>(), scl[1].get<float>(), scl[2].get<float>() };
		newObj->SetTransform(transform);
		// コンポーネントの読み込み
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

// ローカルスレッドでのオブジェクト追加
void Scene::AddObjectLocal(Object* obj){
	if (obj) {
		_ToBeAdded.push_back(obj);
	}
}

// オブジェクトの非同期追加
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



