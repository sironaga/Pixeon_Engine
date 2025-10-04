#include "SettingManager.h"
#include <nlohmann/json.hpp>
#include <fstream>

#define CONFIG_FILE_PATH "SceneRoot/config/config.json"

SettingManager* SettingManager::instance = nullptr;

SettingManager* SettingManager::GetInstance()
{
	if (instance == nullptr)
	{
		instance = new SettingManager();
	}
	return instance;
}

void SettingManager::DestroyInstance()
{
	if (instance != nullptr)
	{
		delete instance;
		instance = nullptr;
	}
}

// Config�̓ǂݍ���
void SettingManager::LoadConfig(){
	// JSON�t�@�C����ǂݍ���
	std::ifstream configFile(CONFIG_FILE_PATH);
	if (!configFile.is_open()) {
		// �t�@�C�������݂��Ȃ��ꍇ�A�f�t�H���g�ݒ���g�p
		return;
	}
	nlohmann::json configJson;
	configFile >> configJson;
	// �ݒ荀�ڂ�ǂݍ���
	if (configJson.contains("AssetsFilePath")) {
		AssetsFilePath = configJson["AssetsFilePath"].get<std::string>();
	}
	if (configJson.contains("ArchiveFilePath")) {
		ArchiveFilePath = configJson["ArchiveFilePath"].get<std::string>();
	}
	if (configJson.contains("SceneFilePath")) {
		SceneFilePath = configJson["SceneFilePath"].get<std::string>();
	}
	if (configJson.contains("bZBuffer")) {
		bZBuffer = configJson["bZBuffer"].get<bool>();
	}
	if (configJson.contains("AutoSaveInterval")) {
		AutoSaveInterval = configJson["AutoSaveInterval"].get<int>();
	}
	if (configJson.contains("BackgroundColor") && configJson["BackgroundColor"].is_array() && configJson["BackgroundColor"].size() == 4) {
		BackgroundColor.x = configJson["BackgroundColor"][0].get<float>();
		BackgroundColor.y = configJson["BackgroundColor"][1].get<float>();
		BackgroundColor.z = configJson["BackgroundColor"][2].get<float>();
		BackgroundColor.w = configJson["BackgroundColor"][3].get<float>();
	}
	if (configJson.contains("ExternelTool")) {
		ExternelTool = configJson["ExternelTool"].get<std::string>();
	}
	
}

void SettingManager::SaveConfig(){
	// �ݒ荀�ڂ�JSON�I�u�W�F�N�g�ɕۑ�
	nlohmann::json configJson;
	configJson["AssetsFilePath"] = AssetsFilePath;
	configJson["ArchiveFilePath"] = ArchiveFilePath;
	configJson["SceneFilePath"] = SceneFilePath;
	configJson["bZBuffer"] = bZBuffer;
	configJson["AutoSaveInterval"] = AutoSaveInterval;
	configJson["BackgroundColor"] = { BackgroundColor.x, BackgroundColor.y, BackgroundColor.z, BackgroundColor.w };
	configJson["ExternelTool"] = ExternelTool;

	// JSON�t�@�C���ɏ�������
	std::ofstream configFile(CONFIG_FILE_PATH);
	if (configFile.is_open()) {
		configFile << configJson.dump(4); // �C���f���g��4�X�y�[�X�ɐݒ肵�ĕۑ�
	}
}
