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

    // ���΃p�X�iAssets ���[�g����̃p�X or �A�[�J�C�u���G���g�����j�Ń��[�h
    bool LoadAsset(const std::string& name, std::vector<uint8_t>& outData);

    // �t���p�X���ړǂݍ��݁iFromSource �p�j�� �L���b�V���L�[�͑��΃p�X�ɐ��K��
    bool LoadAssetFullPath(const std::string& fullPath, std::vector<uint8_t>& outData);

    // �t���p�X���L���b�V���i���e�͓ǂݎ̂āj
    void CacheAssetFullPath(const std::string& fullPath);

    // �z�b�g�����[�h�p�r: �����L���b�V�����폜���čēǍ��i�t���p�X�j
    bool ReloadFromFullPath(const std::string& fullPath);

    void ClearCache();

    // �ꗗ�擾
    std::vector<std::string> ListAssets() const;        // �C���f�b�N�X + �L���b�V�� + (FromSource��) �t�@�C���V�X�e���̈ꗗ
    std::vector<std::string> ListCachedAssets() const;  // �L���b�V���ς݃L�[�ꗗ

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

// �f�B���N�g���ċA�v�����[�h�i�g���q�������t�B���^�A��Ȃ�S�āj
void PreloadRecursive(const std::string& root, const std::vector<std::string>& extsLower);

// �ύX�Ď�
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

// ���΃p�X�ϊ��w���p
#include <filesystem>
#include "SettingManager.h"

inline std::string ToAssetsRelative(const std::string& fullPath) {
    namespace fs = std::filesystem;
    std::string root = SettingManager::GetInstance()->GetAssetsFilePath();
    if (!root.empty()) {
        // �����Z�p���[�^���ꏜ���ifs::relative �̈��艻�j
        while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
    }
    fs::path pFull(fullPath);
    fs::path pRoot(root);
    std::error_code ec;
    auto rel = fs::relative(pFull, pRoot, ec);
    if (!ec) {
        return rel.generic_string(); // ��: "Characters/Hero.fbx"
    }
    return pFull.filename().string();
}