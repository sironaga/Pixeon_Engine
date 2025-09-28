// AssetManager
// アセットのi/oを管理するクラス
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

class AssetManager
{
public:
    enum class LoadMode { FromSource, FromArchive };
    static AssetManager& Instance();

    void SetRoot(const std::string& root);
    void SetLoadMode(LoadMode m);
    bool LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData); // 生バイト取得
    bool Exists(const std::string& logicalName);
    void ClearRawCache();

private:
    AssetManager() = default;
    std::string Normalize(const std::string& name) const;

    std::string m_root;
    LoadMode m_mode = LoadMode::FromSource;
    std::unordered_map<std::string, std::vector<uint8_t>> m_cache;
    mutable std::mutex m_mtx;

	static AssetManager* s_instance;
};

