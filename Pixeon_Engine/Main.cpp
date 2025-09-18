#include "Main.h"
#include "System.h"
#include "AssetsManager.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "SettingManager.h"

HWND ghWnd;
AssetWatcher *watcher;
GameRenderTarget* gGameRenderTarget;
bool bInGame;
std::vector<PostEffectBase*> gPostEffects;

int Init(const EngineConfig& InPut){
	// DirectX11�̏�����
	ghWnd = InPut.hWnd;
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;
	// �ݒ�̓ǂݍ���
	SettingManager::GetInstance()->LoadConfig();
	std::string ArchivePath = SettingManager::GetInstance()->GetArchiveFilePath();
	ArchivePath += "assets.PixAssets";

	//�@�A�Z�b�g�}�l�[�W���[�̋N��
	AssetsManager::GetInstance()->SetLoadMode(AssetsManager::LoadMode::FromSource);
	AssetsManager::GetInstance()->Open(ArchivePath);
	watcher = new AssetWatcher(SettingManager::GetInstance()->GetArchiveFilePath(), "assets.PixAssets",
		[&]() {
			AssetsManager::GetInstance()->Open(ArchivePath);
		}
	);
	// �񓯊��Ď��J�n
	watcher->Start();
	// �Q�[�������_�����O�^�[�Q�b�g�̏�����
	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// GUI�̏�����
	EditrGUI::GetInstance()->Init();
	bInGame = false;
	// �V�[���}�l�[�W���[�̏�����
	SceneManger::GetInstance()->Init();

	return 0;
}

void Update(){
	if(bInGame){
		InGameUpdate();
	}
	else{
		EditeUpdate();
	}
}

void Draw(){
	EditeDraw();
}

void UnInit(){
	watcher->Stop();
	delete watcher;
	SceneManger::GetInstance()->Save();
	SceneManger::DestroyInstance();
	SettingManager::GetInstance()->SaveConfig();
	SettingManager::DestroyInstance();
	DirectX11::DestroyInstance();
}

// ��������

void EditeUpdate(){
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
}

void InGameUpdate(){
	SceneManger::GetInstance()->PlayUpdate();
}

void EditeDraw(){

	gGameRenderTarget->Begin(DirectX11::GetInstance()->GetContext());
	SceneManger::GetInstance()->Draw();
	gGameRenderTarget->End();

	ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void InGamDraw(){

	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

HWND GetWindowHandle(){
	return ghWnd;
}

ID3D11ShaderResourceView* GetGameRender(){
	return gGameRenderTarget->GetShaderResourceView();
}

bool IsInGame(){
	return bInGame;
}

void SetInGame(bool inGame){
	bInGame = inGame;
}

void OpenExplorer(const std::string& path)
{
	ShellExecuteA(
		NULL,           // �E�B���h�E�n���h��
		"open",         // ����iopen��OK�j
		"explorer.exe", // ���s�t�@�C��
		path.c_str(),   // �p�����[�^�i�J�������p�X�j
		NULL,           // �f�B���N�g���iNULL��OK�j
		SW_SHOWNORMAL   // �E�B���h�E�\�����@
	);
}

std::string GetExePath() {
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH) {
		// �擾���s
		return "";
	}
	return std::string(path, length);
}

std::string RemoveExeFromPath(const std::string& exePath)
{
	size_t pos = exePath.find_last_of("\\/");
	if (pos != std::string::npos)
	{
		// �t�@�C�����������������A�t�H���_�p�X�ɂ���
		return exePath.substr(0, pos);
	}
	return exePath; // ��؂肪������Ȃ������ꍇ�͂��̂܂ܕԂ�
}

bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak) {
	std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

	if (!result) return false;

	MessageBoxA(NULL, "�A�[�J�C�u�����J�n���܂����B�����܂ł��΂炭���҂����������B", "���", MB_OK | MB_ICONINFORMATION);

	// �v���Z�X�I���܂őҋ@
	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}

bool RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath)
{
	// �R�}���h���C����: "SceneRoot/Tool/Tool.exe" "SceneRoot/Assets" "SceneRoot/Archive/assets.PixAssets"
	std::string cmd = "\"" + toolExePath + "\" \"" + assetDir + "\" \"" + archivePath + "\"";

	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr,            // lpApplicationName
		(LPSTR)cmd.c_str(), // lpCommandLine
		nullptr,            // lpProcessAttributes
		nullptr,            // lpThreadAttributes
		FALSE,              // bInheritHandles
		CREATE_NO_WINDOW,   // dwCreationFlags
		nullptr,            // lpEnvironment
		nullptr,            // lpCurrentDirectory
		&si,                // lpStartupInfo
		&pi                 // lpProcessInformation
	);

	if (!result) {
		std::string ErrorMsg = "�c�[���̋N���Ɏ��s: " + std::to_string(GetLastError());
		MessageBoxA(NULL, ErrorMsg.c_str(), "�G���[", MB_OK | MB_ICONERROR);
		return false;
	}

	// �N����I���܂őҋ@
	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}