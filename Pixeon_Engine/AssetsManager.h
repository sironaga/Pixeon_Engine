#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <unordered_map>

struct AssetEntry {
    std::string name;
    uint64_t size;
    uint64_t offset;
};

class AssetsManager
{
public:
    static AssetsManager* GetInstance();
    static void DestroyInstance();

    enum class LoadMode {
        FromArchive,
        FromSource,
    };

    bool Open(const std::string& filepath);
    void SetLoadMode(LoadMode mode) { m_loadMode = mode; }

    bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData); // name は相対パス想定
    bool LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData); // 追加
    void CacheAssetFullPath(const std::string& fullPath); // 追加
    void ClearCache();

    std::vector<std::string> ListAssets() const;        // 既存：名前一覧
    std::vector<std::string> ListCachedAssets() const;  // 既存：キャッシュ済

private:
    AssetsManager() {}
    ~AssetsManager() {}

private:
    std::map<std::string, AssetEntry> m_index;
    std::ifstream m_file;
    // キーを相対パスに統一
    std::unordered_map<std::string, std::vector<uint8_t>> m_assetCache;

    static AssetsManager* instance;
    LoadMode m_loadMode = LoadMode::FromArchive;
};

void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower);


#include <functional>
#include <thread>
#include <atomic>

class AssetWatcher {
public:
    using Callback = std::function<void(const std::string& file)>;

    AssetWatcher(const std::string& dir, Callback onChange);
    ~AssetWatcher();

    void Start();
    void Stop();

private:
    void WatchThread();

    std::string m_dir;
    Callback m_callback;
    std::atomic<bool> m_running;
    std::thread m_thread;
};

#include <string>
#include <filesystem>
#include "SettingManager.h"

inline std::string ToAssetsRelative(const std::string& fullPath) {
    namespace fs = std::filesystem;
    std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
    fs::path pFull(fullPath);
    fs::path pRoot(root);
    std::error_code ec;
    auto rel = fs::relative(pFull, pRoot, ec);
    if (!ec) {
        auto s = rel.generic_string();
        return s; // 例: "Characters/Hero.fbx"
    }
    // 失敗時 fallback としてファイル名だけ
    return pFull.filename().string();
}