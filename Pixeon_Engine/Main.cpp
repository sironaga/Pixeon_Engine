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
	SettingManager::GetInstance()->SaveConfig();
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

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void InGamDraw(){

	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void AssetsUpdate() {

}

HWND GetWindowHandle(){
	return ghWnd;
}

ID3D11ShaderResourceView* GetGameRender(){
	return gGameRenderTarget->GetShaderResourceView();
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