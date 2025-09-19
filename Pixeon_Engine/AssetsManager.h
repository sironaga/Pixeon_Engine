// アセット管理クラス
#pragma once
#include <string>
#include <vector>
#include <map>
#include <fstream>

#pragma once
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>

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
public:
    enum class LoadMode {
        FromArchive,
        FromSource,
    };
public:
    bool Open(const std::string& filepath);
    void SetLoadMode(LoadMode mode) { m_loadMode = mode; }
    bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData);
    void CacheAsset(const std::string& filepath);
    void ClearCache();
    std::vector<std::string> ListAssets() const;
private:
    AssetsManager() {}
    ~AssetsManager() {}

private:
    std::map<std::string, AssetEntry> m_index;
    std::ifstream m_file;
    std::unordered_map<std::string, std::vector<uint8_t>> m_assetCache;
private:
    static AssetsManager* instance;
    LoadMode m_loadMode = LoadMode::FromArchive;
};

// ファイル監視クラスの実装
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