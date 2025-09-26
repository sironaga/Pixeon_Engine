#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>

struct AssetEntry {
    std::string name;
    uint64_t size;
    uint64_t offset;
};

class AssetsManager {
public:
    static AssetsManager* GetInstance();
    static void DestroyInstance();

    enum class LoadMode {
        FromArchive,
        FromSource,
    };

    bool Open(const std::string& filepath);
    void SetLoadMode(LoadMode mode) { m_loadMode = mode; }

    // 相対パス（Assets ルートからのパス or アーカイブ内エントリ名）でロード
    bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData);

    // フルパス直接読み込み（FromSource 用）→ キャッシュキーは相対パスに正規化
    bool LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData);

    // フルパスをキャッシュ（内容は読み捨て）
    void CacheAssetFullPath(const std::string& fullPath);

    // ホットリロード用途: 既存キャッシュを削除して再読込（フルパス）
    bool ReloadFromFullPath(const std::string& fullPath);

    void ClearCache();

    // 一覧取得
    std::vector<std::string> ListAssets() const;        // インデックス + キャッシュ + (FromSource時) ファイルシステムの一覧
    std::vector<std::string> ListCachedAssets() const;  // キャッシュ済みキー一覧

private:
    AssetsManager() {}
    ~AssetsManager() {}

private:
    std::map<std::string, AssetEntry> m_index;
    std::ifstream m_file;
    std::unordered_map<std::string, std::vector<uint8_t>> m_assetCache;

    static AssetsManager* instance;
    LoadMode m_loadMode = LoadMode::FromArchive;
};

// ディレクトリ再帰プリロード（拡張子小文字フィルタ、空なら全て）
void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower);

// 変更監視
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
    std::atomic<bool> m_running{ false };
    std::thread m_thread;
};

// 相対パス変換ヘルパ
#include <filesystem>
#include "SettingManager.h"

inline std::string ToAssetsRelative(const std::string& fullPath) {
    namespace fs = std::filesystem;
    std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
    if (!root.empty()) {
        // 末尾セパレータ統一除去（fs::relative の安定化）
        while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    }
    fs::path pFull(fullPath);
    fs::path pRoot(root);
    std::error_code ec;
    auto rel = fs::relative(pFull, pRoot, ec);
    if (!ec) {
        return rel.generic_string(); // 例: "Characters/Hero.fbx"
    }
    return pFull.filename().string();
}