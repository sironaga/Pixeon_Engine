#include "AssetsManager.h"
#include "SettingManager.h"
#include <unordered_map>
#include <unordered_set>

AssetsManager* AssetsManager::instance = nullptr;

AssetsManager* AssetsManager::GetInstance() {
    if (instance == nullptr) {
        instance = new AssetsManager();
    }
    return instance;
}

void AssetsManager::DestroyInstance() {
    if (instance) {
        delete instance;
        instance = nullptr;
    }
}

bool AssetsManager::Open(const std::string& filepath) {
    m_file.open(filepath, std::ios::binary);
    if (!m_file) return false;
    m_index.clear();

    uint32_t fileCount = 0;
    m_file.read(reinterpret_cast<char*>(&fileCount), sizeof(fileCount));
    for (uint32_t i = 0; i < fileCount; ++i) {
        uint32_t nameLen = 0;
        char nameBuf[260] = {};
        uint64_t size = 0, offset = 0;

        m_file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        m_file.read(nameBuf, 260);
        m_file.read(reinterpret_cast<char*>(&size), sizeof(size));
        m_file.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        std::string name(nameBuf, nameLen);
        m_index[name] = { name, size, offset };
    }
    return true;
}

bool AssetsManager::LoadAsset(const std::string& name, std::vector<uint8_t>& outData) {
    auto cacheIt = m_assetCache.find(name);
    if (cacheIt != m_assetCache.end()) {
        outData = cacheIt->second;
        return true;
    }

    if (m_loadMode == LoadMode::FromArchive) {
        auto it = m_index.find(name);
        if (it == m_index.end()) return false;
        m_file.seekg(it->second.offset, std::ios::beg);
        outData.resize(static_cast<size_t>(it->second.size));
        m_file.read(reinterpret_cast<char*>(outData.data()), it->second.size);
        // �L���b�V���ɕۑ�
        m_assetCache[name] = outData;
        return true;
    }
    else {
        std::ifstream file(SettingManager::GetInstance()->GetAssetsFilePath() + name, std::ios::binary);
        if (!file) return false;
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        outData.resize(size);
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char*>(outData.data()), size);
        // �L���b�V���ɕۑ�
        m_assetCache[name] = outData;
        return true;
    }
}

void AssetsManager::CacheAsset(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return;
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);

    // �t�@�C�������o�iAssets/�ȍ~�̖��O���L���b�V���L�[�Ɂj
    auto pos = filepath.find_last_of("/\\");
    std::string name = (pos != std::string::npos) ? filepath.substr(pos + 1) : filepath;
    m_assetCache[name] = std::move(data);
}

void AssetsManager::ClearCache() {
    m_assetCache.clear();
}

std::vector<std::string> AssetsManager::ListAssets() const {
    std::unordered_set<std::string> allNames;
    // �A�[�J�C�u�̃A�Z�b�g����ǉ�
    for (const auto& p : m_index) {
        allNames.insert(p.first);
    }
    // �L���b�V���̃A�Z�b�g�����ǉ��i�d���͖��������j
    for (const auto& p : m_assetCache) {
        allNames.insert(p.first);
    }
    // vector�ɋl�ߑւ��ĕԂ�
    return std::vector<std::string>(allNames.begin(), allNames.end());
}

// �A�Z�b�g�Ǘ��N���X
#include <windows.h>
#include <filesystem>
#include <chrono>

AssetWatcher::AssetWatcher(const std::string& dir, Callback onChange)
    : m_dir(dir), m_callback(onChange), m_running(false) {
}

AssetWatcher::~AssetWatcher() {
    Stop();
}

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
        m_dir.c_str(), TRUE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hChange == INVALID_HANDLE_VALUE) return;

    while (m_running) {
        DWORD wait = WaitForSingleObject(hChange, 500);
        if (wait == WAIT_OBJECT_0) {
            // �t�H���_���̑S�t�@�C�����`�F�b�N
            for (const auto& entry : std::filesystem::directory_iterator(m_dir)) {
                if (entry.is_regular_file()) {
                    if (m_callback) m_callback(entry.path().string()); // �t�@�C���p�X���R�[���o�b�N
                }
            }
            FindNextChangeNotification(hChange);
        }
    }
    FindCloseChangeNotification(hChange);
}

