#include "AssetManager.h"
#include <fstream>
#include <Windows.h>
#include "IMGUI/imgui.h"
#include "ErrorLog.h"

AssetManager* AssetManager::s_instance = nullptr;

AssetManager* AssetManager::Instance()
{
    if (!s_instance) {
        s_instance = new AssetManager();
    }
    return s_instance;
}

void AssetManager::DeleteInstance()
{
    if (s_instance) {
        s_instance->UnInit();
        delete s_instance;
        s_instance = nullptr;
    }
}

void AssetManager::UnInit()
{
    StopAutoSync();
    ClearRawCache();
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_recentChanges.clear();
    }
}

AssetManager::~AssetManager() {
    StopAutoSync();
}

void AssetManager::SetRoot(const std::string& root) { m_root = root; }
void AssetManager::SetLoadMode(LoadMode m) { m_mode = m; }

std::string AssetManager::Normalize(const std::string& name) const {
    std::string s = name;
    for (auto& c : s) if (c == '\\') c = '/';
    return s;
}

bool AssetManager::Exists(const std::string& logicalName) {
    std::string norm = Normalize(logicalName);
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        if (m_cache.find(norm) != m_cache.end()) return true;
    }
    std::filesystem::path p = std::filesystem::path(m_root) / norm;
    return std::filesystem::exists(p);
}

bool AssetManager::LoadAsset(const std::string& logicalName, std::vector<uint8_t>& outData) {
    std::string norm = Normalize(logicalName);
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        auto it = m_cache.find(norm);
        if (it != m_cache.end()) {
            outData = it->second;
            return true;
        }
    }

    std::filesystem::path p = std::filesystem::path(m_root) / norm;
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) {
		ErrorLogger::Instance().LogError("AssetManager", "Failed to open asset: " + norm);
        return false;
    }
    ifs.seekg(0, std::ios::end);
    size_t sz = (size_t)ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    outData.resize(sz);
    ifs.read((char*)outData.data(), sz);
    if (!ifs) {
		ErrorLogger::Instance().LogError("AssetManager", "Failed to read asset: " + norm);
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_cache[norm] = outData;
    }
    return true;
}

void AssetManager::ClearRawCache() {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_cache.clear();
    m_fileMeta.clear();
}

void AssetManager::PushChange(ChangeType type, const std::string& path) {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_recentChanges.size() >= kMaxRecentChanges)
        m_recentChanges.pop_front();
    m_recentChanges.push_back({ type, path, m_scanCount.load() });
}

void AssetManager::StartAutoSync(std::chrono::milliseconds interval, bool recursive) {
    if (m_watchRunning.load()) return;
    if (m_root.empty()) {
		ErrorLogger::Instance().LogError("AssetManager", "StartAutoSync failed: root not set.");
        return;
    }
    m_interval = interval;
    m_recursive = recursive;
    m_watchRunning = true;
    m_watchThread = std::thread(&AssetManager::WatchLoop, this);
    OutputDebugStringA("[AssetManager] AutoSync started.\n");
}

void AssetManager::StopAutoSync() {
    if (!m_watchRunning.load()) return;
    m_watchRunning = false;
    if (m_watchThread.joinable()) m_watchThread.join();
    OutputDebugStringA("[AssetManager] AutoSync stopped.\n");
}

void AssetManager::WatchLoop() {
    // 初回スキャンで現存ファイルをメタに登録（ロードは要求時）
    PerformScan(); // 初回に差分反映（追加扱いで即ロードしたければこのままでOK）
    while (m_watchRunning.load()) {
        auto t0 = std::chrono::steady_clock::now();
        PerformScan();
        auto t1 = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        m_lastScanDurationMs.store((uint64_t)elapsed);
        std::this_thread::sleep_for(m_interval);
    }
}

void AssetManager::PerformScan() {
    using namespace std::filesystem;

    std::unordered_map<std::string, FileMeta> current;
    std::vector<std::string> additions;
    std::vector<std::string> modifications;
    std::vector<std::string> deletions;

    const path rootPath(m_root);
    if (!exists(rootPath)) {
        OutputDebugStringA("[AssetManager] Root path does not exist.\n");
        return;
    }

    auto scanStart = std::chrono::steady_clock::now();

    // ディレクトリ列挙
    std::error_code ec;
    if (m_recursive) {
        for (recursive_directory_iterator it(rootPath, ec), end; it != end && !ec; ++it) {
            if (!it->is_regular_file()) continue;
            auto rel = relative(it->path(), rootPath, ec);
            if (ec) continue;
            std::string relStr = Normalize(rel.generic_string());
            FileMeta meta;
            meta.size = (uint64_t)file_size(it->path(), ec);
            meta.writeTime = last_write_time(it->path(), ec);
            current[relStr] = meta;
        }
    }
    else {
        for (directory_iterator it(rootPath, ec), end; it != end && !ec; ++it) {
            if (!it->is_regular_file()) continue;
            auto rel = relative(it->path(), rootPath, ec);
            if (ec) continue;
            std::string relStr = Normalize(rel.generic_string());
            FileMeta meta;
            meta.size = (uint64_t)file_size(it->path(), ec);
            meta.writeTime = last_write_time(it->path(), ec);
            current[relStr] = meta;
        }
    }

    // 差分判定
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        // 追加 / 変更
        for (auto& kv : current) {
            auto itOld = m_fileMeta.find(kv.first);
            if (itOld == m_fileMeta.end()) {
                additions.push_back(kv.first);
            }
            else {
                if (kv.second.size != itOld->second.size ||
                    kv.second.writeTime != itOld->second.writeTime) {
                    modifications.push_back(kv.first);
                }
            }
        }
        // 削除
        for (auto& old : m_fileMeta) {
            if (current.find(old.first) == current.end()) {
                deletions.push_back(old.first);
            }
        }
    }

    // 追加/変更ファイルのロード（ロックを外した状態で IO）
    for (auto& add : additions) {
        std::vector<uint8_t> dummy;
        if (LoadAsset(add, dummy)) {
            PushChange(ChangeType::Added, add);
        }
        else {
            PushChange(ChangeType::ReloadFailed, add);
        }
    }
    for (auto& mod : modifications) {
        std::vector<uint8_t> dummy;
        if (LoadAsset(mod, dummy)) {
            PushChange(ChangeType::Modified, mod);
        }
        else {
            PushChange(ChangeType::ReloadFailed, mod);
        }
    }
    // 削除適用
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        for (auto& del : deletions) {
            m_cache.erase(del);
            m_fileMeta.erase(del);
            PushChange(ChangeType::Removed, del);
        }
        // ファイルメタ更新
        for (auto& kv : current) {
            m_fileMeta[kv.first] = kv.second;
        }
    }

    m_lastDiffAdds.store(additions.size());
    m_lastDiffMods.store(modifications.size());
    m_lastDiffRemoves.store(deletions.size());
    m_scanCount.fetch_add(1);
}

void AssetManager::DrawDebugGUI()
{
    std::lock_guard<std::mutex> lk(m_mtx);
    ImGui::TextUnformatted("AssetManager");
    ImGui::Separator();
    ImGui::Text("Root: %s", m_root.c_str());
    ImGui::Text("Mode: %s", (m_mode == LoadMode::FromSource) ? "FromSource" : "FromArchive");
    ImGui::Text("Cached Raw Files: %zu", m_cache.size());
    ImGui::Text("AutoSync: %s", m_watchRunning.load() ? "Running" : "Stopped");
    ImGui::Text("ScanCount: %llu", (unsigned long long)m_scanCount.load());
    ImGui::Text("LastDiff A=%llu M=%llu R=%llu",
        (unsigned long long)m_lastDiffAdds.load(),
        (unsigned long long)m_lastDiffMods.load(),
        (unsigned long long)m_lastDiffRemoves.load());
    ImGui::Text("LastScanDuration: %llu ms", (unsigned long long)m_lastScanDurationMs.load());

    if (!m_watchRunning.load()) {
        if (ImGui::Button("Start AutoSync")) {
            // デフォルト 1000ms で開始
            // （GUI側で設定できるよう拡張可）
            // ロック外で呼ぶため一旦抜ける工夫をするなら別途フラグでもOK
        }
    }
    else {
        if (ImGui::Button("Stop AutoSync")) {
            // 外部で実際の StopAutoSync 呼び出し推奨（ここで直接呼ぶのも可）
        }
    }

    static char filter[128] = "";
    ImGui::InputText("Filter (substring)", filter, sizeof(filter));

    if (ImGui::Button("Clear Raw Cache")) {
        m_cache.clear();
        m_fileMeta.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Change Log")) {
        m_recentChanges.clear();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Recent Changes:");
    ImGui::BeginChild("AssetManagerChanges", ImVec2(0, 120), true);
    for (auto it = m_recentChanges.rbegin(); it != m_recentChanges.rend(); ++it) {
        const char* t = "";
        switch (it->type) {
        case ChangeType::Added: t = "ADD"; break;
        case ChangeType::Removed: t = "DEL"; break;
        case ChangeType::Modified: t = "MOD"; break;
        case ChangeType::ReloadFailed: t = "ERR"; break;
        }
        if (filter[0] && it->path.find(filter) == std::string::npos) continue;
        ImGui::Text("[%s] %s (scan=%llu)", t, it->path.c_str(), (unsigned long long)it->timestampFrame);
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextUnformatted("Cache Entries:");
    ImGui::BeginChild("AssetManagerCacheList", ImVec2(0, 160), true);
    for (auto& kv : m_cache) {
        if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
        ImGui::Text("%s (size=%zu bytes)", kv.first.c_str(), kv.second.size());
    }
    ImGui::EndChild();
}

std::vector<std::string> AssetManager::GetCachedAssetNames(bool onlyModelExt) const {
    std::vector<std::string> result;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        result.reserve(m_cache.size());
        for (auto& kv : m_cache) {
            if (!onlyModelExt) {
                result.push_back(kv.first);
            }
            else {
                std::string lower = kv.first;
                for (auto& c : lower) c = (char)tolower(c);
                auto hasExt = [&](const char* ext)->bool {
                    size_t Ls = lower.size(), Le = std::strlen(ext);
                    if (Ls < Le) return false;
                    return lower.compare(Ls - Le, Le, ext) == 0;
                    };
                if (hasExt(".fbx") || hasExt(".obj") || hasExt(".gltf") || hasExt(".glb"))
                    result.push_back(kv.first);
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> AssetManager::GetCachedTextureNames() const{
    static const char* exts[] = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".hdr" };
    std::vector<std::string> result;
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        result.reserve(m_cache.size());
        for (auto& kv : m_cache) {
            std::string lower = kv.first;
            for (auto& c : lower) c = (char)tolower(c);
            auto hasExt = [&](const char* ext)->bool {
                size_t Ls = lower.size(), Le = std::strlen(ext);
                if (Ls < Le) return false;
                return lower.compare(Ls - Le, Le, ext) == 0;
                };
            for (auto* e : exts) {
                if (hasExt(e)) { result.push_back(kv.first); break; }
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}
