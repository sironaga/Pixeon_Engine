// 設定管理マネージャー
// シングルトンパターン
// jsonにて保存

#pragma once
#include <string>


class SettingManager
{
public:
	static SettingManager* GetInstance();
	static void DestroyInstance();

	void LoadConfig();
	void SaveConfig();


public:
	// 設定項目のゲッターとセッター
	std::string GetAssetsFilePath() const { return AssetsFilePath; }
	void SetAssetsFilePath(const std::string& path) { AssetsFilePath = path; }

	std::string GetArchiveFilePath() const { return ArchiveFilePath; }
	void SetArchiveFilePath(const std::string& path) { ArchiveFilePath = path; }

	std::string GetSceneFilePath() const { return SceneFilePath; }
	void SetSceneFilePath(const std::string& path) { SceneFilePath = path; }


private:
	static SettingManager* instance;
private:
	// 設定項目
	std::string AssetsFilePath	= "SceneRoot/Assets/";
	std::string ArchiveFilePath = "SceneRoot/Archive/";
	std::string SceneFilePath	= "SceneRoot/Scene/";




	SettingManager() {}
	~SettingManager() {}
};

