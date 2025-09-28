// サウンドリソースの管理

#pragma once
#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class SoundManager {
public:
    static SoundManager& Instance();
    std::shared_ptr<SoundResource> LoadOrGet(const std::string& logicalName, bool streaming = false);
    void GarbageCollect();

private:
    SoundManager() = default;
    std::shared_ptr<SoundResource> LoadInternal(const std::string& logicalName, bool streaming);

    struct Entry { std::weak_ptr<SoundResource> weak; uint64_t lastUse = 0; size_t bytes = 0; bool streaming = false; };
    std::unordered_map<std::string, Entry> m_cache;
    uint64_t m_frame = 0;
    std::mutex m_mtx;

	static SoundManager* s_instance;
};