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


void SettingManager::LoadConfig(){
	// JSONファイルを読み込む
	std::ifstream configFile(CONFIG_FILE_PATH);
	if (!configFile.is_open()) {
		// ファイルが存在しない場合、デフォルト設定を使用
		return;
	}
	nlohmann::json configJson;
	configFile >> configJson;
	// 設定項目を読み込む
	if (configJson.contains("AssetsFilePath")) {
		AssetsFilePath = configJson["AssetsFilePath"].get<std::string>();
	}
	if (configJson.contains("ArchiveFilePath")) {
		ArchiveFilePath = configJson["ArchiveFilePath"].get<std::string>();
	}
	if (configJson.contains("SceneFilePath")) {
		SceneFilePath = configJson["SceneFilePath"].get<std::string>();
	}

}

void SettingManager::SaveConfig(){
	// 設定項目をJSONオブジェクトに保存
	nlohmann::json configJson;
	configJson["AssetsFilePath"] = AssetsFilePath;
	configJson["ArchiveFilePath"] = ArchiveFilePath;
	configJson["SceneFilePath"] = SceneFilePath;
	// JSONファイルに書き込む
	std::ofstream configFile(CONFIG_FILE_PATH);
	if (configFile.is_open()) {
		configFile << configJson.dump(4); // インデントを4スペースに設定して保存
	}
}
