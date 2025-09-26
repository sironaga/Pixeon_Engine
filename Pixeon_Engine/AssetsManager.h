#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <shared_mutex>
#include <list>
#include <array>
#include <cstdint>

#include "ArchiveFormat.h" // フォーマット定義
#include "HashUtill.h"
#include "SettingManager.h"
#include <filesystem>

// 標準拡張版 AssetEntry
struct AssetEntry {
    std::string name;
    uint64_t    originalSize = 0;
    uint64_t    storedSize = 0;
    uint64_t    offset = 0;
    uint32_t    crc32 = 0;
    uint16_t    compressionMethod = 0;
    uint16_t    flags = 0;
    std::array<uint8_t, 32> contentHash{}; // SHA-256 (展開後対象)
    uint32_t    generation = 0;
    uint32_t    refCount = 0;
};

class AssetsManager {
public:
    static AssetsManager* GetInstance();
    static void DestroyInstance();

    enum class LoadMode {
        FromArchive,
        FromSource,
    };

    bool Open(const std::string& filepath); // v1/v2 両対応
    void Close();
    void SetLoadMode(LoadMode mode);

    bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData);
    bool LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData);
    void CacheAssetFullPath(const std::string& fullPath);
    bool ReloadFromFullPath(const std::string& fullPath);

    void ClearCache();

    std::vector<std::string> ListAssets() const;
    std::vector<std::string> ListCachedAssets() const;

    void SetMaxCacheBytes(size_t bytes);

    // 参照管理
    class AssetHandle {
    public:
        AssetHandle() = default;
        AssetHandle(AssetsManager* mgr, std::string key, uint32_t gen)
            : m_mgr(mgr), m_key(std::move(key)), m_generation(gen) {
        }
        AssetHandle(const AssetHandle& o);
        AssetHandle& operator=(const AssetHandle& o);
        AssetHandle(AssetHandle&& o) noexcept;
        AssetHandle& operator=(AssetHandle&& o) noexcept;
        ~AssetHandle();

        bool IsValid() const;
        const std::string& Key() const { return m_key; }
        uint32_t Generation() const { return m_generation; }

    private:
        void Acquire();
        void Release();
        AssetsManager* m_mgr = nullptr;
        std::string m_key;
        uint32_t m_generation = 0;
    };

    AssetHandle AcquireHandle(const std::string& name);
    uint32_t GetGeneration(const std::string& name) const;

    // オプション: CRC/Hash 検証を有効化
    void EnableCRCCheck(bool enable) { m_checkCRC = enable; }
    void EnableHashCheck(bool enable) { m_checkHash = enable; }

private:
    AssetsManager() = default;
    ~AssetsManager() { Close(); }

    std::string NormalizeKey(const std::string& path) const;

    void TouchLRU(const std::string& key);
    void EnforceCacheLimit();

    bool ReadArchiveV1(std::ifstream& f);
    bool ReadArchiveV2(std::ifstream& f, const PakHeader& header);

    bool DecompressIfNeeded(const AssetEntry& e,
        const std::vector<uint8_t>& inStored,
        std::vector<uint8_t>& outDecompressed);

private:
    std::map<std::string, AssetEntry> m_index;
    std::ifstream m_file;

    struct CacheItem {
        std::vector<uint8_t> data;
        size_t size = 0;
        std::list<std::string>::iterator lruIt;
    };
    std::unordered_map<std::string, CacheItem> m_assetCache;
    std::list<std::string> m_lru;
    size_t m_cacheBytes = 0;
    size_t m_cacheMaxBytes = SIZE_MAX;

    static AssetsManager* instance;
    LoadMode m_loadMode = LoadMode::FromArchive;

    mutable std::shared_mutex m_mutex;

    bool m_checkCRC = true;
    bool m_checkHash = true;
};

// 再帰プリロード
void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower);

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

inline std::string ToAssetsRelative(const std::string& fullPath) {
    namespace fs = std::filesystem;
    std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
    while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    fs::path pFull(fullPath);
    fs::path pRoot(root);
    std::error_code ec;
    auto rel = fs::relative(pFull, pRoot, ec);
    std::string r = (!ec) ? rel.generic_string() : pFull.filename().string();
    for (auto& c : r) if (c == '\\') c = '/';
    std::string lower;
    lower.reserve(r.size());
    for (char c : r) lower.push_back((char)tolower((unsigned char)c));
    return lower;
}