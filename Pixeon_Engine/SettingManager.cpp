#include "SettingManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

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
	// JSON�t�@�C����ǂݍ���
	std::ifstream configFile(CONFIG_FILE_PATH);
	if (!configFile.is_open()) {
		// �t�@�C�������݂��Ȃ��ꍇ�A�f�t�H���g�ݒ���g�p
		return;
	}
	
	try {
		nlohmann::json configJson;
		configFile >> configJson;
		configFile.close();
		
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
	catch (const nlohmann::json::exception& e) {
		// JSON�p�[�X�G���[�͌x���Ƃ��ăf�t�H���g�ݒ��g�p
		MessageBox(nullptr, ("�ݒ�t�@�C���̓ǂݍ���G���[�A�f�t�H���g�ݒ���g�p���܂�: " + std::string(e.what())).c_str(), "�ݒ�G���[", MB_OK | MB_ICONWARNING);
	}
	catch (const std::exception& e) {
		// ���̑��̃G���[
		MessageBox(nullptr, ("�ݒ�t�@�C�����[�h�G���[: " + std::string(e.what())).c_str(), "�ݒ�G���[", MB_OK | MB_ICONWARNING);
	}
	
}

void SettingManager::SaveConfig(){
	try {
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
		// �ڃf�B���N�g���̕\�ڋ���
		std::filesystem::path configPath(CONFIG_FILE_PATH);
		std::filesystem::path configDir = configPath.parent_path();
		
		if (!std::filesystem::exists(configDir)) {
			try {
				std::filesystem::create_directories(configDir);
			}
			catch (const std::filesystem::filesystem_error& e) {
				MessageBox(nullptr, ("�ݒ�f�B���N�g���̍쐬�Ɏ��s: " + std::string(e.what())).c_str(), "�ݒ��Z�[�u�G���[", MB_OK | MB_ICONERROR);
				return;
			}
		}
		
		std::ofstream configFile(CONFIG_FILE_PATH);
		if (configFile.is_open()) {
			configFile << configJson.dump(4); // �C���f���g��4�X�y�[�X�ɐݒ肵�ĕۑ�
			if (configFile.fail()) {
				MessageBox(nullptr, "�ݒ�t�@�C���̏�������s���܂���", "�ݒ��Z�[�u�G���[", MB_OK | MB_ICONERROR);
			}
			configFile.close();
		} else {
			MessageBox(nullptr, "�ݒ�t�@�C�����J���܂���", "�ݒ��Z�[�u�G���[", MB_OK | MB_ICONERROR);
		}
	}
	catch (const nlohmann::json::exception& e) {
		// JSON�V���A���C�Y�G���[
		MessageBox(nullptr, ("�ݒ�JSON�쐬�G���[: " + std::string(e.what())).c_str(), "�ݒ��Z�[�u�G���[", MB_OK | MB_ICONERROR);
	}
	catch (const std::exception& e) {
		// ���̑��̃G���[
		MessageBox(nullptr, ("�ݒ��Z�[�u�G���[: " + std::string(e.what())).c_str(), "�ݒ��Z�[�u�G���[", MB_OK | MB_ICONERROR);
	}
}
