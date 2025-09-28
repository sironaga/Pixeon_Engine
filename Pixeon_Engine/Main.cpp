#include "Main.h"
#include "System.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "Object.h"

#include "AssetManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "SoundManager.h"

HWND ghWnd;
GameRenderTarget* gGameRenderTarget;
bool bInGame;
std::vector<PostEffectBase*> gPostEffects;
std::vector<Object*> gContentsObjects;

int Init(const EngineConfig& InPut){

	// DirectX11の初期化
	ghWnd = InPut.hWnd;
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;
	// 設定の読み込み
	SettingManager::GetInstance()->LoadConfig();
	//　アセットマネージャーの起動
	AssetManager::Instance();
	TextureManager::Instance();
	ModelManager::Instance();
	SoundManager::Instance();
	// 非同期監視開始
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);

	// ゲームレンダリングターゲットの初期化
	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	// GUIの初期化
	EditrGUI::GetInstance()->Init();
	bInGame = false;
	// シーンマネージャーの初期化
	SceneManger::GetInstance()->Init();
	// ContentsObjectsの初期化
	gContentsObjects.clear();
	// シェーダーマネージャーの初期化
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	ComponentManager::GetInstance()->Init();

	return 0;
}

void Update(){
	if(bInGame){
		InGameUpdate();}
	else{
		EditeUpdate();}
}

void Draw(){
	EditeDraw();
}

void UnInit(){

	AssetManager::Instance()->StopAutoSync();
	SceneManger::GetInstance()->Save();
	SceneManger::DestroyInstance();
	SettingManager::GetInstance()->SaveConfig();
	SettingManager::DestroyInstance();
	DirectX11::DestroyInstance();
}

// 内部処理

void EditeUpdate(){
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
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

std::vector<Object*> GetContentsObjects(){
	return gContentsObjects;
}

void AddContentsObject(Object* obj){
	Object* CloneObj;
	CloneObj = obj->Clone();
	gContentsObjects.push_back(CloneObj);
}

void RemoveContentsObject(Object* obj){
	auto it = std::remove(gContentsObjects.begin(), gContentsObjects.end(), obj);
	if(it != gContentsObjects.end()) {
		gContentsObjects.erase(it, gContentsObjects.end());
		obj->UInit();
		delete obj;
	}
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

std::string RemoveExeFromPath(const std::string& exePath)
{
	size_t pos = exePath.find_last_of("\\/");
	if (pos != std::string::npos)
	{
		// ファイル名部分を除去し、フォルダパスにする
		return exePath.substr(0, pos);
	}
	return exePath; // 区切りが見つからなかった場合はそのまま返す
}

bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak) {
	std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

	if (!result) return false;

	MessageBoxA(NULL, "アーカイブ化を開始しました。完了までしばらくお待ちください。", "情報", MB_OK | MB_ICONINFORMATION);

	// プロセス終了まで待機
	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}

bool RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath)
{
	// コマンドライン例: "SceneRoot/Tool/Tool.exe" "SceneRoot/Assets" "SceneRoot/Archive/assets.PixAssets"
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
		std::string ErrorMsg = "ツールの起動に失敗: " + std::to_string(GetLastError());
		MessageBoxA(NULL, ErrorMsg.c_str(), "エラー", MB_OK | MB_ICONERROR);
		return false;
	}

	// 起動後終了まで待機
	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}