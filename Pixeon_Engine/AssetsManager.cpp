#include "AssetsManager.h"
#include "SettingManager.h"
#include <filesystem>
#include <Windows.h>

AssetsManager* AssetsManager::instance = nullptr;

// ... GetInstance / DestroyInstance / Open はそのまま(省略) ...

bool AssetsManager::LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData) {
    // フルパス指定で直接読み込み（FromSource 前提）
    std::ifstream file(fullPath, std::ios::binary);
    if (!file) return false;
    file.seekg(0, std::ios::end);
    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    outData.resize(size);
    file.read(reinterpret_cast<char*>(outData.data()), size);
    auto rel = ToAssetsRelative(fullPath);
    m_assetCache[rel] = outData;
    return true;
}

void AssetsManager::CacheAssetFullPath(const std::string& fullPath) {
    namespace fs = std::filesystem;
    if (fs::is_directory(fullPath)) return;
    std::vector<uint8_t> dummy;
    LoadAssetFullPath(fullPath, dummy);
}

bool AssetsManager::LoadAsset(const std::string& name, std::vector<uint8_t>& outData) {
    // 既に相対パスキーがキャッシュにあれば利用
    auto it = m_assetCache.find(name);
    if (it != m_assetCache.end()) {
        outData = it->second;
        return true;
    }
    if (m_loadMode == LoadMode::FromArchive) {
        auto it2 = m_index.find(name);
        if (it2 == m_index.end()) return false;
        m_file.seekg(it2->second.offset, std::ios::beg);
        outData.resize((size_t)it2->second.size);
        m_file.read(reinterpret_cast<char*>(outData.data()), it2->second.size);
        m_assetCache[name] = outData;
        return true;
    }
    else {
        // FromSource: AssetsRoot + name
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
        std::string full = root + name;
        return LoadAssetFullPath(full, outData);
    }
}

static bool MatchExt(const std::string& extLower, const std::vector<std::string>& extsLower) {
    if (extsLower.empty()) return true;
    for (auto& e : extsLower) if (extLower == e) return true;
    return false;
}

void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower)
{
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
