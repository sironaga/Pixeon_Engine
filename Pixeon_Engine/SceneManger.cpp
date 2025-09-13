#include "SceneManger.h"
#include "Scene.h"

// �V�[���f�B���N�g���̃p�X
#define SCENE_DIR "bin/Scenes/"

SceneManger* SceneManger::instance = nullptr;

SceneManger* SceneManger::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new SceneManger();
	}
	return instance;
}

void SceneManger::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

void SceneManger::Init(){
	_sceneList = ListSceneFiles();
	for (const auto& fileName : _sceneList)
	{
		std::string sceneName = fileName.substr(0, fileName.find_last_of('.'));
		if (CreateAndRegisterScene(sceneName)){
			std::cout << "Registered scene: " << sceneName << std::endl;
		}
		else{
			std::cout << "Scene already exists: " << sceneName << std::endl;
		}
	}
}

// �V�[���J�n
void SceneManger::BeginPlay(){
	if (_currentScene) {
		_currentScene->SaveToFile();
		_currentScene->BeginPlay();
	}
}

// �X�V
void SceneManger::EditUpdate(){
	// �V�[���̐؂�ւ�
	if (_nextScene) {
		if (_currentScene)delete _currentScene;
		_currentScene = _nextScene;
		_currentScene->Init();
		_currentScene->LoadToFile();
	}
	// �X�V
	if (_currentScene)_currentScene->EditUpdate();
}

// �X�V
void SceneManger::PlayUpdate(){
	if (_nextScene) {
		if (_currentScene)delete _currentScene;
		_currentScene = _nextScene;
		_currentScene->Init();
		_currentScene->LoadToFile();
		_currentScene->BeginPlay();
	}
	if (_currentScene)_currentScene->PlayUpdate();
}

// �`��
void SceneManger::Draw(){
	if (_currentScene)_currentScene->Draw();
}

// �V�[���̕ύX
void SceneManger::ChangeScene(std::string SceneName){
	auto it = _SceneCreators.find(SceneName);
	if (it != _SceneCreators.end()) {
		_nextScene = it->second();
	}
}

// �V�K�V�[���̍쐬�Ɠo�^
bool SceneManger::CreateAndRegisterScene(std::string SceneName) {

	for (const auto& Name : _sceneList){
		if (Name == SceneName){
			return false;
		}
	}

	RegisterScene(SceneName, [SceneName]() -> Scene* {
		Scene* newScene = new Scene();
		newScene->SetName(SceneName);
		return newScene;
		});
	return true;
}

// �V�[���̓o�^
void SceneManger::RegisterScene(std::string Name, std::function<Scene* ()> creator){
	_SceneCreators[Name] = creator;
}

// �t�@�C�����̎擾
std::vector<std::string> SceneManger::ListSceneFiles(){
	std::vector<std::string> sceneFiles;
	std::string sceneDir = SCENE_DIR;
	std::string searchPath = sceneDir + "\\*.scene";
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		// �V�[���t�@�C����������Ȃ��ꍇ
		CreateAndRegisterScene("SampleScene");
	}

	do {
		if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			sceneFiles.push_back(findData.cFileName);
		}
	} while (FindNextFileA(hFind, &findData));
	FindClose(hFind);

	return sceneFiles;
}

// �t�@�C�����̕ύX
bool SceneManger::RenameFileInDirectory(const std::string& oldName, const std::string& newName){
	std::string oldPath = std::string(SCENE_DIR) + oldName;
	std::string newPath = std::string(SCENE_DIR) + newName;
	if (std::rename(oldPath.c_str(), newPath.c_str()) != 0) {
		// ���l�[�����s
		return false;
	}
	return true;
}
