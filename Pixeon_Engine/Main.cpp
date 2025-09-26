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
// �ύX�Ď�: �P���ăL���b�V������
//---------------------------------------------
static const std::vector<std::string> kPreloadExts = {
    "fbx","obj","gltf","glb",
    "png","jpg","jpeg","dds","tga","hdr",
    "wav","ogg"
};

// �g���q���������Ŏ擾
static std::string GetLowerExt(const std::string& path) {
    auto p = path.find_last_of('.');
    if (p == std::string::npos) return "";
    std::string e = path.substr(p + 1);
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return e;
}

// �Ď��ύX�R�[���o�b�N
void OnAssetChanged(const std::string& filepath) {
    // �ʃt�@�C�����ăL���b�V��
    AssetsManager::GetInstance()->CacheAssetFullPath(filepath);
}

int Init(const EngineConfig& InPut) {

    // DirectX ������
    ghWnd = InPut.hWnd;
    HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
    if (FAILED(hr)) return -1;

    // �ݒ胍�[�h
    SettingManager::GetInstance()->LoadConfig();

    // �A�Z�b�g�}�l�[�W�� FromSource ���[�h
    AssetsManager::GetInstance()->SetLoadMode(AssetsManager::LoadMode::FromSource);

    // �V�K: �ċA�v�����[�h�i�N�����ꊇ�j
    {
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        PreloadRecursive(root, kPreloadExts);
    }

    // �Ď��J�n
    watcher = new AssetWatcher(SettingManager::GetInstance()->GetAssetsFilePath(), OnAssetChanged);
    watcher->Start();

    // Game RenderTarget ������
    gGameRenderTarget = new GameRenderTarget();
    gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);

    // GUI ������
    EditrGUI::GetInstance()->Init();
    bInGame = false;

    // �V�[���}�l�[�W��������
    SceneManger::GetInstance()->Init();

    // ContentsObjects ������
    gContentsObjects.clear();

    // �V�F�[�_�}�l�[�W��������
    ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
    ComponentManager::GetInstance()->Init();

    // ZBuffer �ݒ�
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
    // �V�F�[�_�̃z�b�g�����[�h (����)
    ShaderManager::GetInstance()->UpdateAndCompileShaders();

    // GUI
    EditrGUI::GetInstance()->Update();

    // �V�[���ҏW�X�V
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
    MessageBoxA(NULL, "�A�[�J�C�u�������J�n���܂����B�����܂ł��҂����������B", "���", MB_OK | MB_ICONINFORMATION);
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
        std::string ErrorMsg = "�c�[���̋N���Ɏ��s: " + std::to_string(GetLastError());
        MessageBoxA(NULL, ErrorMsg.c_str(), "�G���[", MB_OK | MB_ICONERROR);
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (exitCode == 0);
}