#include "Main.h"
#include "System.h"
#include "AssetsManager.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "Object.h"


HWND ghWnd;
AssetWatcher* watcher;
GameRenderTarget* gGameRenderTarget;
bool bInGame;
std::vector<PostEffectBase*> gPostEffects;
std::vector<Object*> gContentsObjects;
bool bZBuffer = true;

//---------------------------------------------
// 変更監視: 単発再キャッシュ方式
//---------------------------------------------
static const std::vector<std::string> kPreloadExts = {
    "fbx","obj","gltf","glb",
    "png","jpg","jpeg","dds","tga","hdr",
    "wav","ogg"
};

// 拡張子を小文字で取得
static std::string GetLowerExt(const std::string& path) {
    auto p = path.find_last_of('.');
    if (p == std::string::npos) return "";
    std::string e = path.substr(p + 1);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

// 監視変更コールバック
void OnAssetChanged(const std::string& filepath) {
    // 個別ファイルを再キャッシュ
    AssetsManager::GetInstance()->CacheAssetFullPath(filepath);
}

int Init(const EngineConfig& InPut) {

    // DirectX 初期化
    ghWnd = InPut.hWnd;
    HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
    if (FAILED(hr)) return -1;

    // 設定ロード
    SettingManager::GetInstance()->LoadConfig();

    // アセットマネージャ FromSource モード
    AssetsManager::GetInstance()->SetLoadMode(AssetsManager::LoadMode::FromSource);

    // 新規: 再帰プリロード（起動時一括）
    {
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        PreloadRecursive(root, kPreloadExts);
    }

    // 監視開始
    watcher = new AssetWatcher(SettingManager::GetInstance()->GetAssetsFilePath(), OnAssetChanged);
    watcher->Start();

    // Game RenderTarget 初期化
    gGameRenderTarget = new GameRenderTarget();
    gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);

    // GUI 初期化
    EditrGUI::GetInstance()->Init();
    bInGame = false;

    // シーンマネージャ初期化
    SceneManger::GetInstance()->Init();

    // ContentsObjects 初期化
    gContentsObjects.clear();

    // シェーダマネージャ初期化
    ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
    ComponentManager::GetInstance()->Init();

    // ZBuffer 設定
    bZBuffer = SettingManager::GetInstance()->GetZBuffer();
    RenderTarget* pRTV = DirectX11::GetInstance()->GetDefaultRTV();
    DepthStencil* pDSV = bZBuffer ? DirectX11::GetInstance()->GetDefaultDSV() : nullptr;
    gGameRenderTarget->SetRenderZBuffer(bZBuffer);
    DirectX11::GetInstance()->SetRenderTargets(1, &pRTV, pDSV);

    return 0;
}

void Update() {
    if (bInGame) {
        InGameUpdate();
    }
    else {
        EditeUpdate();
    }
}

void Draw() {
    if (bZBuffer != SettingManager::GetInstance()->GetZBuffer()) {
        bZBuffer = SettingManager::GetInstance()->GetZBuffer();
        RenderTarget* pRTV = DirectX11::GetInstance()->GetDefaultRTV();
        DepthStencil* pDSV = bZBuffer ? DirectX11::GetInstance()->GetDefaultDSV() : nullptr;
        gGameRenderTarget->SetRenderZBuffer(bZBuffer);
        DirectX11::GetInstance()->SetRenderTargets(1, &pRTV, pDSV);
    }
    EditeDraw();
}

void UnInit() {
    if (watcher) {
        watcher->Stop();
        delete watcher;
        watcher = nullptr;
    }
    AssetsManager::GetInstance()->ClearCache();
    AssetsManager::DestroyInstance();
    SceneManger::GetInstance()->Save();
    SceneManger::DestroyInstance();
    SettingManager::GetInstance()->SaveConfig();
    SettingManager::DestroyInstance();
    DirectX11::DestroyInstance();
}

// ----------------

void EditeUpdate() {
    // シェーダのホットリロード (既存)
    ShaderManager::GetInstance()->UpdateAndCompileShaders();

    // GUI
    EditrGUI::GetInstance()->Update();

    // シーン編集更新
    SceneManger::GetInstance()->EditUpdate();
}

void InGameUpdate() {
    SceneManger::GetInstance()->PlayUpdate();
}

void EditeDraw() {
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

void InGamDraw() {
    DirectX11::GetInstance()->BeginDraw();
    SceneManger::GetInstance()->Draw();
    DirectX11::GetInstance()->EndDraw();
}

HWND GetWindowHandle() {
    return ghWnd;
}

ID3D11ShaderResourceView* GetGameRender() {
    return gGameRenderTarget->GetShaderResourceView();
}

bool IsInGame() {
    return bInGame;
}

void SetInGame(bool inGame) {
    bInGame = inGame;
}

std::vector<Object*> GetContentsObjects() {
    return gContentsObjects;
}

void AddContentsObject(Object* obj) {
    Object* CloneObj = obj->Clone();
    gContentsObjects.push_back(CloneObj);
}

void RemoveContentsObject(Object* obj) {
    auto it = std::remove(gContentsObjects.begin(), gContentsObjects.end(), obj);
    if (it != gContentsObjects.end()) {
        gContentsObjects.erase(it, gContentsObjects.end());
        obj->UInit();
        delete obj;
    }
}

void OpenExplorer(const std::string& path) {
    ShellExecuteA(nullptr, "open", "explorer.exe", path.c_str(), nullptr, SW_SHOWNORMAL);
}

std::string GetExePath() {
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return "";
    }
    return std::string(path, length);
}

std::string RemoveExeFromPath(const std::string& exePath) {
    size_t pos = exePath.find_last_of("\\/");
    if (pos != std::string::npos) {
        return exePath.substr(0, pos);
    }
    return exePath;
}

bool CallAssetPacker(const std::string& toolPath, const std::string& assetDir, const std::string& outputPak) {
    std::string cmd = "\"" + toolPath + "\" \"" + assetDir + "\" \"" + outputPak + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL result = CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!result) return false;
    MessageBoxA(NULL, "アーカイブ生成を開始しました。完了までお待ちください。", "情報", MB_OK | MB_ICONINFORMATION);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0);
}

bool RunArchiveTool(const std::string& toolExePath, const std::string& assetDir, const std::string& archivePath) {
    std::string cmd = "\"" + toolExePath + "\" \"" + assetDir + "\" \"" + archivePath + "\"";
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    BOOL result = CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!result) {
        std::string ErrorMsg = "ツールの起動に失敗: " + std::to_string(GetLastError());
        MessageBoxA(NULL, ErrorMsg.c_str(), "エラー", MB_OK | MB_ICONERROR);
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0);
}