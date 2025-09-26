#include "AssetsManager.h"
#include "SettingManager.h"
#include <filesystem>
#include <Windows.h>
#include <unordered_set>
#include <algorithm>

AssetsManager* AssetsManager::instance = nullptr;

AssetsManager* AssetsManager::GetInstance() {
    if (!instance) instance = new AssetsManager();
    return instance;
}

void AssetsManager::DestroyInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

bool AssetsManager::Open(const std::string& filepath) {
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

bool AssetsManager::LoadAsset(const std::string& name, std::vector<uint8_t>& outData) {
    // キャッシュ優先
    if (auto it = m_assetCache.find(name); it != m_assetCache.end()) {
        outData = it->second;
        return true;
    }

    if (m_loadMode == LoadMode::FromArchive) {
        auto it = m_index.find(name);
        if (it == m_index.end()) return false;
        m_file.seekg(it->second.offset, std::ios::beg);
        outData.resize((size_t)it->second.size);
        m_file.read(reinterpret_cast<char*>(outData.data()), it->second.size);
        m_assetCache[name] = outData;
        return true;
    }
    else {
        // FromSource
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
        std::string full = root + name;
        return LoadAssetFullPath(full, outData);
    }
}

bool AssetsManager::LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData) {
    std::ifstream file(fullPath, std::ios::binary);
    if (!file) {
        std::string dbg = "[AssetsManager] Open failed: " + fullPath + "\n";
        OutputDebugStringA(dbg.c_str());
        return false;
    }
    file.seekg(0, std::ios::end);
    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    outData.resize(size);
    file.read(reinterpret_cast<char*>(outData.data()), size);

    // 相対キーに正規化
    std::string rel = ToAssetsRelative(fullPath);
    m_assetCache[rel] = outData;
    return true;
}

void AssetsManager::CacheAssetFullPath(const std::string& fullPath) {
    namespace fs = std::filesystem;
    if (fs::is_directory(fullPath)) return;
    std::vector<uint8_t> dummy;
    LoadAssetFullPath(fullPath, dummy);
}

bool AssetsManager::ReloadFromFullPath(const std::string& fullPath) {
    std::string rel = ToAssetsRelative(fullPath);
    m_assetCache.erase(rel);
    std::vector<uint8_t> dummy;
    return LoadAssetFullPath(fullPath, dummy);
}

void AssetsManager::ClearCache() {
    m_assetCache.clear();
}

std::vector<std::string> AssetsManager::ListAssets() const {
    std::unordered_set<std::string> all;
    // アーカイブエントリ
    for (auto& p : m_index) all.insert(p.first);
    // キャッシュ済
    for (auto& p : m_assetCache) all.insert(p.first);
    // FromSource 時は実ファイル探索
    if (m_loadMode == LoadMode::FromSource) {
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
        try {
            for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
                if (!e.is_regular_file()) continue;
                auto rel = std::filesystem::relative(e.path(), root);
                all.insert(rel.generic_string());
            }
        }
        catch (...) {}
    }
    return { all.begin(), all.end() };
}

std::vector<std::string> AssetsManager::ListCachedAssets() const {
    std::vector<std::string> v;
    v.reserve(m_assetCache.size());
    for (auto& kv : m_assetCache) v.push_back(kv.first);
    return v;
}

// ---- PreloadRecursive ----
static bool MatchExt(const std::string& extLower, const std::vector<std::string>& extsLower) {
    if (extsLower.empty()) return true;
    for (auto& e : extsLower) if (extLower == e) return true;
    return false;
}

void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower) {
    namespace fs = std::filesystem;
    try {
        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (!ext.empty() && ext.front() == '.') ext.erase(0, 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (!MatchExt(ext, extsLower)) continue;
            AssetsManager::GetInstance()->CacheAssetFullPath(entry.path().string());
        }
    }
    catch (...) {
        // アクセス不可は無視
    }
}

// ---- AssetWatcher ----
AssetWatcher::AssetWatcher(const std::string& dir, Callback onChange)
    : m_dir(dir), m_callback(onChange) {
}

AssetWatcher::~AssetWatcher() {
    Stop();
}

void AssetWatcher::Start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&AssetWatcher::WatchThread, this);
}

void AssetWatcher::Stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}

void AssetWatcher::WatchThread() {
    HANDLE hChange = FindFirstChangeNotificationA(
        m_dir.c_str(), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hChange == INVALID_HANDLE_VALUE) return;

    while (m_running) {
        DWORD wait = WaitForSingleObject(hChange, 500);
        if (wait == WAIT_OBJECT_0) {
            // 上位ディレクトリ直下のみ（再帰監視したい場合は拡張要）
            for (const auto& entry : std::filesystem::directory_iterator(m_dir)) {
                if (!entry.is_regular_file()) continue;
                if (m_callback) m_callback(entry.path().string());
            }
            FindNextChangeNotification(hChange);
        }
    }
    FindCloseChangeNotification(hChange);
}