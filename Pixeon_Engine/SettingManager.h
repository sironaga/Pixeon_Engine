// �ݒ�Ǘ��}�l�[�W���[
// �V���O���g���p�^�[��
// json�ɂĕۑ�

#pragma once
#include <string>
#include "System.h"

class SettingManager
{
public:

	static SettingManager* GetInstance();
	static void DestroyInstance();

	void LoadConfig();
	void SaveConfig();

public:
	// �ݒ荀�ڂ̃Q�b�^�[�ƃZ�b�^�[
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

	DirectX::XMFLOAT4 GetBackgroundColor() const { return BackgroundColor; }
	void SetBackgroundColor(const DirectX::XMFLOAT4& color) { BackgroundColor = color; }

	std::string GetExternelToolPath() const { return ExternelTool; }
	void SetExternelToolPath(const std::string& path) { ExternelTool = path; }

	std::string GetShaderFilePath() const { return ShaderFilePath; }
	void SetShaderFilePath(const std::string& path) { ShaderFilePath = path; }

	std::string GetCSOFilePath() const { return CSOFilePath; }
	void SetCSOFilePath(const std::string& path) { CSOFilePath = path; }

private:
	static SettingManager* instance;
private:
	// �ݒ荀��
	std::string AssetsFilePath	= "SceneRoot/Assets";
	std::string ArchiveFilePath = "SceneRoot/Archive";
	std::string SceneFilePath	= "SceneRoot/Scene";
	std::string PackingTool		= "SceneRoot/Tool/Asset packaging tool.exe";
	std::string ExternelTool	= "SceneRoot/Tool/IN/";
	std::string ShaderFilePath	= "SceneRoot/Shader/hlsl/";
	std::string CSOFilePath		= "SceneRoot/Shader/cso/";
	DirectX::XMFLOAT4 BackgroundColor = DirectX::XMFLOAT4(0.1f, 0.1f, 0.1f,1.0f);
	
	bool bZBuffer = true;
	int AutoSaveInterval = 5; // �����ۑ��Ԋu�i���j

	SettingManager() {}
	~SettingManager() {}
};

