// アセット管理クラスの実装
#include "AssetsManager.h"
#include "SettingManager.h"

AssetsManager* AssetsManager::instance = nullptr;

AssetsManager* AssetsManager::GetInstance(){
	if (instance == nullptr) {
		instance = new AssetsManager();
	}
	return instance;
}

void AssetsManager::DestroyInstance(){
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

bool AssetsManager::Open(const std::string& filepath){
    m_file.open(filepath, std::ios::binary);
    if (!m_file) return false;
    m_index.clear();

    uint32_t fileCount = 0;
    m_file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t nameLen = 0;
        char nameBuf[260] = {};
        uint64_t size = 0, offset = 0;

        m_file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        m_file.read(nameBuf, 260);
        m_file.read(reinterpret_cast<char*>(&size), sizeof(size));
        m_file.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        std::string name(nameBuf, nameLen);
        m_index[name] = { name, size, offset };
    }
    return true;
}

bool AssetsManager::LoadAsset(const std::string& name, std::vector<uint8_t>& outData){
    if (m_loadMode == LoadMode::FromArchive) {
        // PixAssetsから抽出
        auto it = m_index.find(name);
        if (it == m_index.end()) return false;
        m_file.seekg(it->second.offset, std::ios::beg);
        outData.resize(static_cast<size_t>(it->second.size));
        m_file.read(reinterpret_cast<char*>(outData.data()), it->second.size);
        return true;
    }
    else {
        // 元データ（Assetsフォルダ）から直接読み込み
        std::ifstream file(SettingManager::GetInstance()->GetAssetsFilePath() + name, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        outData.resize(size);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(outData.data()), size);
        return true;
    }
}

std::vector<std::string> AssetsManager::ListAssets() const{
    std::vector<std::string> list;
    for (const auto& p : m_index) list.push_back(p.first);
    return list;
}

// アセット管理クラス
#include <windows.h>
#include <chrono>
#include <filesystem>
AssetWatcher::AssetWatcher(const std::string& dir, const std::string& file, Callback onChange)
    : m_dir(dir), m_file(file), m_callback(onChange), m_running(false) {}

AssetWatcher::~AssetWatcher(){
    Stop();
}

void AssetWatcher::Start(){
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&AssetWatcher::WatchThread, this);
}

void AssetWatcher::Stop(){
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void AssetWatcher::WatchThread(){
    HANDLE hChange = FindFirstChangeNotificationA(
        m_dir.c_str(), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hChange == INVALID_HANDLE_VALUE) return;

    // 最初のタイムスタンプを取得
    std::filesystem::path filePath = m_dir + "/" + m_file;
    if (!std::filesystem::exists(filePath))return;
    auto lastWrite = std::filesystem::last_write_time(filePath);
    while (m_running) {
        DWORD wait = WaitForSingleObject(hChange, 500); // 500ms毎にチェック
        if (wait == WAIT_OBJECT_0) {
            // ファイルのタイムスタンプが変わったか確認
            auto nowWrite = std::filesystem::last_write_time(filePath);
            if (nowWrite != lastWrite) {
                lastWrite = nowWrite;
                if (m_callback) m_callback(); // 変更検知コールバック
            }
            FindNextChangeNotification(hChange);
        }
        // 途中でStop()された場合break
    }
    FindCloseChangeNotification(hChange);
}

