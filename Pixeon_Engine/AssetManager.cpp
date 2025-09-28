#include "AssetManager.h"
#include <filesystem>
#include <fstream>
#include <Windows.h>

AssetManager* AssetManager::s_instance = nullptr;

AssetManager& AssetManager::Instance()
{
	if (!s_instance) {
		s_instance = new AssetManager();
	}
	return *s_instance;
}

// 設定
void AssetManager::SetRoot(const std::string& root) { m_root = root; }
void AssetManager::SetLoadMode(LoadMode m) { m_mode = m; }

// 正規化
std::string AssetManager::Normalize(const std::string& name) const{
	std::string s = name;
	for (auto& c : s) if (c == '\\') c = '/';
	return s;
}

// 存在確認
bool AssetManager::Exists(const std::string& logicalName){
	std::string norm = Normalize(logicalName);
	{
		std::lock_guard<std::mutex> lk(m_mtx);
		if (m_cache.find(norm) != m_cache.end()) return true;
	}
	std::filesystem::path p = std::filesystem::path(m_root) / norm;
	return std::filesystem::exists(p);
}

// 読み込み
bool AssetManager::LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData) {
    std::string norm = Normalize(logicalName);
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto it = m_cache.find(norm);
        if (it != m_cache.end()) {
            outData = it->second;
            return true;
        }
    }
    // FromSource のみ実装（Archiveは後で）
    std::filesystem::path p = std::filesystem::path(m_root) / norm;
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) {
        OutputDebugStringA(("[AssetsManager] Open failed: " + p.string() + "\n").c_str());
        return false;
    }
    ifs.seekg(0, std::ios::end);
    size_t sz = (size_t)ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    outData.resize(sz);
    ifs.read((char*)outData.data(), sz);
    if (!ifs) {
        OutputDebugStringA(("[AssetsManager] Read failed: " + norm + "\n").c_str());
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_cache[norm] = outData;
    }
    return true;
}

// キャッシュクリア
void AssetManager::ClearRawCache() {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_cache.clear();
}