// モデルの共有リソースを管理するシングルトンクラス

#pragma once
#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class ModelManager {
public:
    static ModelManager& Instance();
    std::shared_ptr<ModelSharedResource> LoadOrGet(const std::string& logicalName);
    void GarbageCollect();

private:
    ModelManager() = default;
    std::shared_ptr<ModelSharedResource> LoadInternal(const std::string& logicalName);

    struct Entry { std::weak_ptr<ModelSharedResource> weak; uint64_t lastUse = 0; size_t gpuBytes = 0; };
    std::unordered_map<std::string, Entry> m_cache;
    uint64_t m_frame = 0;
    std::mutex m_mtx;
	static ModelManager* s_instance;
};