// �e�N�X�`���A�Z�b�g�Ǘ��N���X
#pragma once
#include "AssetTypes.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class TextureManager {
public:
    static TextureManager& Instance();
    std::shared_ptr<TextureResource> LoadOrGet(const std::string& logicalName);
    void SetMemoryBudget(size_t bytes) { m_budget = bytes; }
    void GarbageCollect(); // ���������i�K�v�Ȃ��� LRU ��ǉ��j

private:
    TextureManager() = default;
    std::shared_ptr<TextureResource> LoadInternal(const std::string& logicalName);

    struct Entry {
        std::weak_ptr<TextureResource> weak;
        uint64_t lastUse = 0;
        size_t   bytes = 0;
    };
    std::unordered_map<std::string, Entry> m_cache;

    size_t    m_budget = 512ull * 1024 * 1024; // �\�Z�i���󖢎g�p�j
    uint64_t  m_frame = 0;
    std::mutex m_mtx;

    static TextureManager* s_instance;
};