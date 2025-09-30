#include "Main.h"
#include "System.h"
#include "AssetManager.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "Object.h"

// グローバル変数
HWND ghWnd;
GameRenderTarget* gGameRenderTarget;
bool bInGame;
std::vector<PostEffectBase*> gPostEffects;
std::vector<Object*> gContentsObjects;

int Init(const EngineConfig& InPut){

	// COM の初期化 (WIC/DirectXTex のため必須)
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;
	// Wndハンドルのコピー
	ghWnd = InPut.hWnd;
	// DirectX の初期化
	hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}
	//　Configの読み込み
	SettingManager::GetInstance()->LoadConfig();
	// アセットマネージャーの設定
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);
	// レンダラーターゲットの作成
	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	gGameRenderTarget->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
	// GUI の初期化
	EditrGUI::GetInstance()->Init();
	bInGame = false;
	// シーンマネージャーの初期化
	SceneManger::GetInstance()->Init();
	// コンテンツの初期化
	gContentsObjects.clear();
	// シェーダーマネージャーの初期化
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	// コンポーネントマネージャーの初期化
	ComponentManager::GetInstance()->Init();

	return 0;
}

void Update(){
	// ZBufferの設定更新
	gGameRenderTarget->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());

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
	
	CoUninitialize();
}


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
		NULL,
		"open",        
		"explorer.exe", 
		path.c_str(),  
		NULL,       
		SW_SHOWNORMAL   

	);
}

std::string GetExePath() {
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
	if (length == 0 || length == MAX_PATH) {
		return "";
	}
	return std::string(path, length);
}

std::string RemoveExeFromPath(const std::string& exePath)
{
	size_t pos = exePath.find_last_of("\\/");
	if (pos != std::string::npos){
		return exePath.substr(0, pos);
	}
	return exePath;
}

bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak) {
	std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
	STARTUPINFOA si = { sizeof(si) };
	PROCESS_INFORMATION pi;
	BOOL result = CreateProcessA(
		nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
		CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

	if (!result) return false;

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}

bool RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath)
{
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
		return false;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return (exitCode == 0);
}