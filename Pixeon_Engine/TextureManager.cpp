#include "TextureManager.h"
#include "AssetManager.h"
#include "System.h"
#include "DirectXTex/DirectXTex.h"
#include <algorithm>
#include <winerror.h>
#include <Windows.h>
#include "IMGUI/imgui.h"

TextureManager* TextureManager::s_instance = nullptr;

TextureManager* TextureManager::Instance() {
    if (!s_instance) {
        s_instance = new TextureManager();
    }
    return s_instance;
}

std::shared_ptr<TextureResource> TextureManager::LoadOrGet(const std::string& logicalName) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_frame++;

    auto it = m_cache.find(logicalName);
    if (it != m_cache.end()) {
        if (auto sp = it->second.weak.lock()) {
            it->second.lastUse = m_frame;
            return sp;
        }
    }

    auto tex = LoadInternal(logicalName);
    if (tex) {
        Entry e;
        e.weak = tex;
        e.lastUse = m_frame;
        e.bytes = tex->gpuBytes;
        m_cache[logicalName] = e;
    }
    return tex;
}

std::shared_ptr<TextureResource> TextureManager::LoadInternal(const std::string& logicalName) {
    std::vector<uint8_t> data;
    if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
        OutputDebugStringA(("[TextureManager] Raw load failed: " + logicalName + "\n").c_str());
        return nullptr;
    }

    // 拡張子取得
    std::string ext;
    if (auto p = logicalName.find_last_of('.'); p != std::string::npos) {
        ext = logicalName.substr(p + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }

    DirectX::ScratchImage img;
    HRESULT hr = E_FAIL;

    // DirectXTex 1.9.0対応の正しい型を使用
    if (ext == "dds") {
        hr = DirectX::LoadFromDDSMemory(
            data.data(),
            data.size(),
            DirectX::DDS_FLAGS::DDS_FLAGS_NONE,
            nullptr,
            img);
    }
    else if (ext == "tga") {
        hr = DirectX::LoadFromTGAMemory(data.data(), data.size(), nullptr, img);
    }
    else if (ext == "hdr") {
        hr = DirectX::LoadFromHDRMemory(data.data(), data.size(), nullptr, img);
    }
    else {
        // WICフラグも正しい型を使用
        hr = DirectX::LoadFromWICMemory(
            data.data(),
            data.size(),
            DirectX::WIC_FLAGS::WIC_FLAGS_NONE,
            nullptr,
            img);
    }

    if (FAILED(hr)) {
        OutputDebugStringA(("[TextureManager] Decode failed: " + logicalName + "\n").c_str());
        return nullptr;
    }

    // MipMap生成（DirectXTex 1.9.0対応）
    if (img.GetMetadata().mipLevels <= 1) {
        DirectX::ScratchImage mip;

        // GenerateMipMapsの正しいシグネチャを使用
        hr = DirectX::GenerateMipMaps(
            img.GetImages(),
            img.GetImageCount(),
            img.GetMetadata(),
            DirectX::TEX_FILTER_FLAGS::TEX_FILTER_DEFAULT,
            0,  // levels (0 = full chain)
            mip);

        if (SUCCEEDED(hr)) {
            img = std::move(mip);
        }
    }

    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) {
        OutputDebugStringA("[TextureManager] Device null\n");
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = DirectX::CreateShaderResourceView(
        dev,
        img.GetImages(),
        img.GetImageCount(),
        img.GetMetadata(),
        srv.GetAddressOf()
    );

    if (FAILED(hr)) {
        OutputDebugStringA(("[TextureManager] CreateSRV failed: " + logicalName + "\n").c_str());
        return nullptr;
    }

    auto tex = std::make_shared<TextureResource>();
    tex->name = logicalName;
    tex->srv = srv;
    tex->width = static_cast<uint32_t>(img.GetMetadata().width);
    tex->height = static_cast<uint32_t>(img.GetMetadata().height);
    tex->gpuBytes = tex->width * tex->height * 4ull;

    OutputDebugStringA(("[TextureManager] Load OK: " + logicalName + "\n").c_str());
    return tex;
}

void TextureManager::GarbageCollect() {
    std::lock_guard<std::mutex> lk(m_mtx);
    for (auto it = m_cache.begin(); it != m_cache.end(); ) {
        if (it->second.weak.expired()) {
            it = m_cache.erase(it);
        }
        else {
            ++it;
        }
    }
}

void TextureManager::DrawDebugGUI() {
    std::lock_guard<std::mutex> lk(m_mtx);
    ImGui::TextUnformatted("TextureManager");
    ImGui::Separator();
    size_t alive = 0;
    size_t totalBytes = 0;
    for (auto& kv : m_cache) {
        if (auto sp = kv.second.weak.lock()) {
            alive++;
            totalBytes += kv.second.bytes;
        }
    }
    ImGui::Text("Cached Entries: %zu (alive=%zu)", m_cache.size(), alive);
    ImGui::Text("GPU Approx Total: %.2f MB", totalBytes / (1024.0 * 1024.0));
    ImGui::Text("Memory Budget: %.2f MB (unused control)", m_budget / (1024.0 * 1024.0));
    static char filter[128] = "";
    ImGui::InputText("Filter", filter, sizeof(filter));

    if (ImGui::Button("GC (Dead Only)")) {
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second.weak.expired()) it = m_cache.erase(it);
            else ++it;
        }
    }
    ImGui::Separator();
    ImGui::BeginChild("TextureList", ImVec2(0, 160), true);
    for (auto& kv : m_cache) {
        if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
        bool isAlive = !kv.second.weak.expired();
        ImGui::Text("%s | %s | %zu bytes | lastUse=%llu",
            kv.first.c_str(),
            isAlive ? "alive" : "dead",
            kv.second.bytes,
            (unsigned long long)kv.second.lastUse);
    }
    ImGui::EndChild();
}