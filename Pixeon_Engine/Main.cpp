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
	// DirectX11の初期化
	ghWnd = InPut.hWnd;
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;
	// 設定の読み込み
	SettingManager::GetInstance()->LoadConfig();
	std::string ArchivePath = SettingManager::GetInstance()->GetArchiveFilePath();
	ArchivePath += "assets.PixAssets";

	//　アセットマネージャーの起動
	AssetsManager::GetInstance()->Open(ArchivePath);
	watcher = new AssetWatcher(SettingManager::GetInstance()->GetArchiveFilePath(), "assets.PixAssets",
		[&]() {
			AssetsManager::GetInstance()->Open(ArchivePath);
		}
	);
	// 非同期監視開始
	watcher->Start();
	// ゲームレンダリングターゲットの初期化
	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// GUIの初期化
	EditrGUI::GetInstance()->Init();
	bInGame = false;
	// シーンマネージャーの初期化
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

// 内部処理

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
		NULL,           // ウィンドウハンドル
		"open",         // 動作（openでOK）
		"explorer.exe", // 実行ファイル
		path.c_str(),   // パラメータ（開きたいパス）
		NULL,           // ディレクトリ（NULLでOK）
		SW_SHOWNORMAL   // ウィンドウ表示方法
	);
}

std::string GetExePath() {
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH) {
		// 取得失敗
		return "";
	}
	return std::string(path, length);
}