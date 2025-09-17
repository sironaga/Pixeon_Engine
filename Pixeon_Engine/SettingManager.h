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

	bool GetZBuffer() const { return bZBuffer; }
	void SetZBuffer(bool bEnable) { bZBuffer = bEnable; }

	int GetAutoSaveInterval() const { return AutoSaveInterval; }
	void SetAutoSaveInterval(int interval) { AutoSaveInterval = interval; }

	std::string GetPackingToolFilePath() const { return PackingTool; }
	void SetPackingToolFilePath(const std::string& path) { PackingTool = path; }

private:
	static SettingManager* instance;
private:
	// 設定項目
	std::string AssetsFilePath	= "SceneRoot/Assets";
	std::string ArchiveFilePath = "SceneRoot/Archive";
	std::string SceneFilePath	= "SceneRoot/Scene";
	std::string PackingTool		= "SceneRoot/Tool/Asset packaging tool.exe";

	bool bZBuffer = true;
	int AutoSaveInterval = 5; // 自動保存間隔（分）



	SettingManager() {}
	~SettingManager() {}
};

