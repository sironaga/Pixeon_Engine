#include "AssetsManager.h"
#include <Windows.h>
#include <unordered_set>
#include <algorithm>
#include <iostream>

AssetsManager* AssetsManager::instance = nullptr;

AssetsManager* AssetsManager::GetInstance() {
    if (!instance) instance = new AssetsManager();
    return instance;
}
void AssetsManager::DestroyInstance() {
    delete instance;
    instance = nullptr;
}

void AssetsManager::SetLoadMode(LoadMode mode) {
    std::unique_lock lock(m_mutex);
    m_loadMode = mode;
}

std::string AssetsManager::NormalizeKey(const std::string& path) const {
    std::string r = path;
    for (auto& c : r) if (c == '\\') c = '/';
    std::string lower;
    lower.reserve(r.size());
    for (char c : r) lower.push_back((char)tolower((unsigned char)c));
    return lower;
}

bool AssetsManager::Open(const std::string& filepath) {
    std::unique_lock lock(m_mutex);
    Close();
    m_file.open(filepath, std::ios::binary);
    if (!m_file) {
        OutputDebugStringA("[AssetsManager] Open archive failed.\n");
        return false;
    }

    PakHeader header{};
    m_file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!m_file) return false;

    if (memcmp(header.magic, "PIXPAK\0", 8) != 0) {
        OutputDebugStringA("[AssetsManager] Invalid magic.\n");
        return false;
    }
    if (header.version == 1) {
        // v1 はヘッダ読み直し後 v1専用パーサ
        m_file.seekg(0, std::ios::beg);
        return ReadArchiveV1(m_file);
    }
    else if (header.version == 2) {
        // v2
        m_file.seekg(0, std::ios::beg); // 再度読みたいなら
        m_file.read(reinterpret_cast<char*>(&header), sizeof(header));
        return ReadArchiveV2(m_file, header);
    }
    else {
        OutputDebugStringA("[AssetsManager] Unsupported version.\n");
        return false;
    }
}

bool AssetsManager::ReadArchiveV1(std::ifstream& f) {
    // 旧形式:先頭に fileCount
    f.seekg(0, std::ios::beg);
    uint32_t fileCount = 0;
    f.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    if (!f) return false;
    m_index.clear();
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t nameLen = 0;
        char nameBuf[260] = {};
        uint64_t size = 0, offset = 0;
        f.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        if (nameLen >= 260) return false;
        f.read(nameBuf, 260);
        f.read(reinterpret_cast<char*>(&size), sizeof(size));
        f.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        if (!f) return false;
        std::string name(nameBuf, nameLen);
        name = NormalizeKey(name);
        AssetEntry e;
        e.name = name;
        e.originalSize = size;
        e.storedSize = size;
        e.offset = offset;
        e.crc32 = 0;
        e.compressionMethod = 0;
        e.flags = 0;
        m_index[name] = e;
    }
    return true;
}

bool AssetsManager::ReadArchiveV2(std::ifstream& f, const PakHeader& header) {
    m_index.clear();
    f.seekg(header.tocOffset, std::ios::beg);
    for (uint32_t i = 0; i < header.fileCount; ++i) {
        uint16_t nameLen = 0;
        f.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        if (!f || nameLen == 0 || nameLen > 4096) return false;
        std::string name(nameLen, '\0');
        f.read(name.data(), nameLen);
        uint8_t compression = 0;
        uint8_t reservedC[3]{};
        f.read(reinterpret_cast<char*>(&compression), 1);
        f.read(reinterpret_cast<char*>(reservedC), 3);
        uint32_t crc32 = 0;
        uint64_t originalSize = 0;
        uint64_t storedSize = 0;
        uint64_t offset = 0;
        f.read(reinterpret_cast<char*>(&crc32), sizeof(crc32));
        f.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));
        f.read(reinterpret_cast<char*>(&storedSize), sizeof(storedSize));
        f.read(reinterpret_cast<char*>(&offset), sizeof(offset));
        std::array<uint8_t, 32> hash{};
        f.read(reinterpret_cast<char*>(hash.data()), hash.size());
        if (!f) return false;

        AssetEntry e;
        e.name = NormalizeKey(name);
        e.originalSize = originalSize;
        e.storedSize = storedSize;
        e.offset = offset;
        e.crc32 = crc32;
        e.compressionMethod = compression;
        e.flags = 0; // v2 TOC には flags は未導入（必要なら拡張）
        e.contentHash = hash;
        e.generation = 0;
        e.refCount = 0;
        m_index[e.name] = e;
    }
    return true;
}

void AssetsManager::Close() {
    if (m_file.is_open()) m_file.close();
    m_index.clear();
    m_assetCache.clear();
    m_lru.clear();
    m_cacheBytes = 0;
}

void AssetsManager::SetMaxCacheBytes(size_t bytes) {
    std::unique_lock lock(m_mutex);
    m_cacheMaxBytes = bytes;
    EnforceCacheLimit();
}

void AssetsManager::TouchLRU(const std::string& key) {
    auto it = m_assetCache.find(key);
    if (it == m_assetCache.end()) return;
    m_lru.erase(it->second.lruIt);
    m_lru.push_front(key);
    it->second.lruIt = m_lru.begin();
}

void AssetsManager::EnforceCacheLimit() {
    while (m_cacheBytes > m_cacheMaxBytes && !m_lru.empty()) {
        auto backKey = m_lru.back();
        auto it = m_assetCache.find(backKey);
        if (it != m_assetCache.end()) {
            m_cacheBytes -= it->second.size;
            m_lru.pop_back();
            m_assetCache.erase(it);
        }
        else {
            m_lru.pop_back();
        }
    }
}

bool AssetsManager::DecompressIfNeeded(const AssetEntry& e,
    const std::vector<uint8_t>& inStored,
    std::vector<uint8_t>& outDecompressed) {
    if (e.compressionMethod == 0) {
        outDecompressed = inStored;
        return true;
    }
    // 将来 LZ4 等実装
    OutputDebugStringA("[AssetsManager] Compression not implemented.\n");
    return false;
}

bool AssetsManager::LoadAsset(const std::string& name, std::vector<uint8_t>& outData) {
    std::shared_lock rlock(m_mutex);
    std::string key = NormalizeKey(name);

    if (auto it = m_assetCache.find(key); it != m_assetCache.end()) {
        outData = it->second.data;
        rlock.unlock();
        std::unique_lock wlock(m_mutex);
        TouchLRU(key);
        return true;
    }

    if (m_loadMode == LoadMode::FromArchive) {
        auto it = m_index.find(key);
        if (it == m_index.end()) return false;
        AssetEntry entry = it->second;

        rlock.unlock();
        std::unique_lock wlock(m_mutex);
        m_file.seekg(entry.offset, std::ios::beg);
        std::vector<uint8_t> stored(entry.storedSize);
        if (entry.storedSize > 0) {
            m_file.read(reinterpret_cast<char*>(stored.data()), entry.storedSize);
            if (!m_file) return false;
        }

        // CRC 検証
        if (m_checkCRC && entry.crc32 != 0) {
            HashUtil::InitCRC32();
            uint32_t c = HashUtil::CalcCRC32(stored.data(), stored.size());
            if (c != entry.crc32) {
                OutputDebugStringA("[AssetsManager] CRC mismatch.\n");
                return false;
            }
        }

        // 展開
        std::vector<uint8_t> decompressed;
        if (!DecompressIfNeeded(entry, stored, decompressed)) {
            return false;
        }

        // ハッシュ検証（展開後）
        if (m_checkHash) {
            auto h = HashUtil::SHA256(decompressed);
            if (h != entry.contentHash) {
                OutputDebugStringA("[AssetsManager] SHA256 mismatch.\n");
                // 必要なら return false; -> 今は失敗扱い
                return false;
            }
        }

        outData = std::move(decompressed);

        // originalSize チェック
        if (entry.originalSize != outData.size()) {
            OutputDebugStringA("[AssetsManager] Size mismatch with originalSize.\n");
            // 失敗判定するかはポリシー（今回は続行）
        }

        CacheItem item;
        item.data = outData;
        item.size = outData.size();
        m_lru.push_front(key);
        item.lruIt = m_lru.begin();
        m_assetCache[key] = std::move(item);
        m_cacheBytes += outData.size();
        EnforceCacheLimit();
        return true;
    }
    else {
        rlock.unlock();
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
        std::string full = root + name;
        return LoadAssetFullPath(full, outData);
    }
}

bool AssetsManager::LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData) {
    std::ifstream file(fullPath, std::ios::binary);
    if (!file) {
        std::string dbg = "[AssetsManager] Open failed: " + fullPath + "\n";
        OutputDebugStringA(dbg.c_str());
        return false;
    }
    file.seekg(0, std::ios::end);
    size_t size = (size_t)file.tellg();
    file.seekg(0, std::ios::beg);
    outData.resize(size);
    file.read(reinterpret_cast<char*>(outData.data()), size);
    if (!file) return false;

    std::string rel = ToAssetsRelative(fullPath);
    std::unique_lock lock(m_mutex);
    auto key = NormalizeKey(rel);
    if (auto it = m_assetCache.find(key); it != m_assetCache.end()) {
        m_cacheBytes -= it->second.size;
        m_lru.erase(it->second.lruIt);
        m_assetCache.erase(it);
    }
    CacheItem item;
    item.data = outData;
    item.size = outData.size();
    m_lru.push_front(key);
    item.lruIt = m_lru.begin();
    m_assetCache[key] = std::move(item);
    m_cacheBytes += outData.size();
    EnforceCacheLimit();
    return true;
}

void AssetsManager::CacheAssetFullPath(const std::string& fullPath) {
    namespace fs = std::filesystem;
    if (fs::is_directory(fullPath)) return;
    std::vector<uint8_t> dummy;
    LoadAssetFullPath(fullPath, dummy);
}

bool AssetsManager::ReloadFromFullPath(const std::string& fullPath) {
    std::string rel = ToAssetsRelative(fullPath);
    std::string key = NormalizeKey(rel);
    {
        std::unique_lock lock(m_mutex);
        if (auto it = m_assetCache.find(key); it != m_assetCache.end()) {
            m_cacheBytes -= it->second.size;
            m_lru.erase(it->second.lruIt);
            m_assetCache.erase(it);
        }
        // generation をインクリメント（FromSource の場合のみエントリ生成/更新）
        auto itIdx = m_index.find(key);
        if (itIdx == m_index.end()) {
            AssetEntry e;
            e.name = key;
            e.generation = 1;
            m_index[key] = e;
        }
        else {
            itIdx->second.generation++;
        }
    }
    std::vector<uint8_t> dummy;
    return LoadAssetFullPath(fullPath, dummy);
}

void AssetsManager::ClearCache() {
    std::unique_lock lock(m_mutex);
    m_assetCache.clear();
    m_lru.clear();
    m_cacheBytes = 0;
}

std::vector<std::string> AssetsManager::ListAssets() const {
    std::shared_lock lock(m_mutex);
    std::unordered_set<std::string> all;
    for (auto& p : m_index) all.insert(p.first);
    for (auto& p : m_assetCache) all.insert(p.first);
    if (m_loadMode == LoadMode::FromSource) {
        std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
        if (!root.empty() && root.back() != '/' && root.back() != '\\') root.push_back('/');
        try {
            for (auto& e : std::filesystem::recursive_directory_iterator(root)) {
                if (!e.is_regular_file()) continue;
                auto rel = std::filesystem::relative(e.path(), root);
                auto s = rel.generic_string();
                for (auto& c : s) if (c == '\\') c = '/';
                std::string lower;
                lower.reserve(s.size());
                for (char c : s) lower.push_back((char)tolower((unsigned char)c));
                all.insert(lower);
            }
        }
        catch (...) {}
    }
    return { all.begin(), all.end() };
}

std::vector<std::string> AssetsManager::ListCachedAssets() const {
    std::shared_lock lock(m_mutex);
    std::vector<std::string> v;
    v.reserve(m_assetCache.size());
    for (auto& kv : m_assetCache) v.push_back(kv.first);
    return v;
}

// プリロード
static bool MatchExt(const std::string& extLower, const std::vector<std::string>& extsLower) {
    if (extsLower.empty()) return true;
    for (auto& e : extsLower) if (extLower == e) return true;
    return false;
}
void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower) {
    namespace fs = std::filesystem;
    try {
        for (auto& entry : fs::recursive_directory_iterator(root)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (!ext.empty() && ext.front() == '.') ext.erase(0, 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (!MatchExt(ext, extsLower)) continue;
            AssetsManager::GetInstance()->CacheAssetFullPath(entry.path().string());
        }
    }
    catch (...) {}
}

// ---- AssetWatcher (簡易) ----
AssetWatcher::AssetWatcher(const std::string& dir, Callback onChange)
    : m_dir(dir), m_callback(onChange) {
}
AssetWatcher::~AssetWatcher() { Stop(); }
void AssetWatcher::Start() {
    if (m_running) return;
    m_running = true;
    m_thread = std::thread(&AssetWatcher::WatchThread, this);
}
void AssetWatcher::Stop() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();
}
void AssetWatcher::WatchThread() {
    HANDLE hChange = FindFirstChangeNotificationA(
        m_dir.c_str(), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hChange == INVALID_HANDLE_VALUE) return;
    while (m_running) {
        DWORD wait = WaitForSingleObject(hChange, 500);
        if (wait == WAIT_OBJECT_0) {
            for (const auto& entry : std::filesystem::directory_iterator(m_dir)) {
                if (!entry.is_regular_file()) continue;
                if (m_callback) m_callback(entry.path().string());
            }
            FindNextChangeNotification(hChange);
        }
    }
    FindCloseChangeNotification(hChange);
}

// ---- AssetHandle 実装 ----
AssetsManager::AssetHandle::AssetHandle(const AssetHandle& o)
    : m_mgr(o.m_mgr), m_key(o.m_key), m_generation(o.m_generation) {
    Acquire();
}
AssetsManager::AssetHandle& AssetsManager::AssetHandle::operator=(const AssetHandle& o) {
    if (this == &o) return *this;
    Release();
    m_mgr = o.m_mgr;
    m_key = o.m_key;
    m_generation = o.m_generation;
    Acquire();
    return *this;
}
AssetsManager::AssetHandle::AssetHandle(AssetHandle&& o) noexcept
    : m_mgr(o.m_mgr), m_key(std::move(o.m_key)), m_generation(o.m_generation) {
    o.m_mgr = nullptr;
    o.m_generation = 0;
}
AssetsManager::AssetHandle& AssetsManager::AssetHandle::operator=(AssetHandle&& o) noexcept {
    if (this == &o) return *this;
    Release();
    m_mgr = o.m_mgr;
    m_key = std::move(o.m_key);
    m_generation = o.m_generation;
    o.m_mgr = nullptr;
    o.m_generation = 0;
    return *this;
}
AssetsManager::AssetHandle::~AssetHandle() { Release(); }

void AssetsManager::AssetHandle::Acquire() {
    if (!m_mgr || m_key.empty()) return;
    std::unique_lock lock(m_mgr->m_mutex);
    auto it = m_mgr->m_index.find(m_key);
    if (it != m_mgr->m_index.end()) {
        it->second.refCount++;
    }
}
void AssetsManager::AssetHandle::Release() {
    if (!m_mgr || m_key.empty()) return;
    std::unique_lock lock(m_mgr->m_mutex);
    auto it = m_mgr->m_index.find(m_key);
    if (it != m_mgr->m_index.end()) {
        if (it->second.refCount > 0) it->second.refCount--;
    }
    m_mgr = nullptr;
    m_key.clear();
    m_generation = 0;
}

bool AssetsManager::AssetHandle::IsValid() const {
    if (!m_mgr) return false;
    std::shared_lock lock(m_mgr->m_mutex);
    auto it = m_mgr->m_index.find(m_key);
    if (it == m_mgr->m_index.end()) return false;
    return it->second.generation == m_generation;
}

AssetsManager::AssetHandle AssetsManager::AcquireHandle(const std::string& name) {
    std::unique_lock lock(m_mutex);
    auto key = NormalizeKey(name);
    auto it = m_index.find(key);
    if (it == m_index.end()) {
        return AssetHandle(); // 無効
    }
    // 参照カウント増 → generation は現状維持
    it->second.refCount++;
    return AssetHandle(this, key, it->second.generation);
}

uint32_t AssetsManager::GetGeneration(const std::string& name) const {
    std::shared_lock lock(m_mutex);
    auto key = NormalizeKey(name);
    auto it = m_index.find(key);
    if (it == m_index.end()) return 0;
    return it->second.generation;
}