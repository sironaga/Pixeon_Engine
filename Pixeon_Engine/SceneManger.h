// �V�[���Ǘ��N���X
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
	// �V�[�����X�g
	std::vector<std::string> _sceneList;
	
	Scene* _currentScene = nullptr;
	Scene* _nextScene = nullptr;

	static SceneManger* instance;
};

