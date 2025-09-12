// アセット管理クラス
#pragma once
#include <string>
#include <vector>
#include <map>
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
	bool Open(const std::string& filepath);
	bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData);
	std::vector<std::string> ListAssets() const;
private:
	AssetsManager() {}
	~AssetsManager() {}

private:
	std::map<std::string, AssetEntry> m_index;
	std::ifstream m_file;
private:
	static AssetsManager* instance;
};


#include <thread>
#include <atomic>
#include <functional>

// アセットアーカイブ監視クラス
class AssetWatcher {
public:
	using Callback = std::function<void()>;

	AssetWatcher(const std::string& dir, const std::string& file, Callback onChange);
	~AssetWatcher();

	void Start();
	void Stop();

private:
	void WatchThread();

	std::string m_dir;
	std::string m_file;
	Callback m_callback;
	std::atomic<bool> m_running;
	std::thread m_thread;
};