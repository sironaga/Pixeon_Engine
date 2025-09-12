#include "Scene.h"
#include "Object.h"
#include <thread>
#include <mutex>

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

void Scene::Draw(){

	// オブジェクトの描画
	std::vector<Object*> sortedList = _objects;
	if (true) {
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



