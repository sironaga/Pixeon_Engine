// AssetManager
// アセットのi/oを管理するクラス
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <deque>
#include <filesystem>

class AssetManager
{
public:
    enum class LoadMode { FromSource, FromArchive };
    static AssetManager* Instance();

    void SetRoot(const std::string& root);
    void SetLoadMode(LoadMode m);
    bool LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData); // 生バイト取得
    bool Exists(const std::string& logicalName);
    void ClearRawCache();

    void DrawDebugGUI();

    void StartAutoSync(std::chrono::milliseconds interval = std::chrono::milliseconds(1000),
        bool recursive = true);

	void StopAutoSync();

    bool IsAutoSyncRunning() const { return m_watchRunning.load(); }

    std::vector<std::string> GetCachedAssetNames(bool onlyModelExt = false) const;
private:
    AssetManager() = default;
    ~AssetManager();
    std::string Normalize(const std::string& name) const;

    void WatchLoop();

    void PerformScan();

    struct FileMeta {
        uint64_t size = 0;
        std::filesystem::file_time_type writeTime;
    };

    enum class ChangeType { Added, Removed, Modified, ReloadFailed };
    struct ChangeLog {
        ChangeType type;
        std::string path;
        uint64_t timestampFrame = 0;
    };

    void PushChange(ChangeType type, const std::string& path);

    std::string m_root;
    LoadMode m_mode = LoadMode::FromSource;

    std::unordered_map<std::string, std::vector<uint8_t>> m_cache;
    std::unordered_map<std::string, FileMeta> m_fileMeta;

    std::deque<ChangeLog> m_recentChanges;
    static constexpr size_t kMaxRecentChanges = 64;

    std::thread m_watchThread;
    std::atomic<bool> m_watchRunning{ false };
    std::chrono::milliseconds m_interval{ 1000 };
    bool m_recursive = true;

    std::atomic<uint64_t> m_scanCount{ 0 };
    std::atomic<uint64_t> m_lastDiffAdds{ 0 };
    std::atomic<uint64_t> m_lastDiffRemoves{ 0 };
    std::atomic<uint64_t> m_lastDiffMods{ 0 };
    std::atomic<uint64_t> m_lastScanDurationMs{ 0 };

    mutable std::mutex m_mtx;

	static AssetManager* s_instance;
};

