// シーン管理クラス
#pragma once

#include <vector>
#include <string>

class Scene;

class SceneManger
{
public:
	static SceneManger* GetInstance();
	static void DestroyInstance();

public:


private:
	SceneManger() {}
	~SceneManger() {}

	


private:
	// シーンリスト
	std::vector<std::string> _sceneList;
	
	Scene* _currentScene = nullptr;
	Scene* _nextScene = nullptr;

	static SceneManger* instance;
};

