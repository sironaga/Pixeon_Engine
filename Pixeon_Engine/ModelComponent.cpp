#include "ModelComponent.h"
#include "System.h"
#include "CameraComponent.h"
#include "Scene.h"
#include "EditrGUI.h"
#include "DirectXTex/DirectXTex.h"
#include <algorithm>
#include <chrono>

using namespace DirectX;

constexpr size_t TEXCACHE_LIMIT = 256;

static bool CaseInsensitiveEndsWith(const std::string& s, const std::string& ext) {
    if (s.size() < ext.size()) return false;
    std::string tail = s.substr(s.size() - ext.size());
    std::transform(tail.begin(), tail.end(), tail.begin(), ::tolower);
    std::string lExt = ext;
    std::transform(lExt.begin(), lExt.end(), lExt.begin(), ::tolower);
    return tail == lExt;
}

AdvancedModelComponent::AdvancedModelComponent() {
    _ComponentName = "AdvancedModelComponent";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
    m_unifiedVS = "VS_ModelSkin";
    m_unifiedPS = "PS_ModelPBR";
    for (int i = (int)MeshPartType::HEAD; i <= (int)MeshPartType::OTHER; ++i)
        m_partVisible[(MeshPartType)i] = true;
    m_modelFilter[0] = '\0';
}

AdvancedModelComponent::~AdvancedModelComponent() {
    Clear();
}

void AdvancedModelComponent::ShowErrorMessage(const char* title, const std::string& msg) {
    MessageBoxA(nullptr, msg.c_str(), title, MB_ICONERROR | MB_OK);
}

void AdvancedModelComponent::Init(Object* Prt) { _Parent = Prt; }
void AdvancedModelComponent::UInit() { Clear(); }

void AdvancedModelComponent::Clear() {
    m_resource.reset();
    m_inputLayout.Reset();
    m_loaded = false;
    m_currentAsset.clear();
    m_cachedModelAssets.clear();
    m_cachedAssetSelected = -1;
    m_selectedCachedAssetName.clear();
}

bool AdvancedModelComponent::LoadModel(const std::string& assetName, bool force) {
    if (m_loaded && !force && assetName == m_currentAsset) return true;

    Clear();
    m_resource = std::make_shared<ModelRuntimeResource>();
    if (!m_resource->LoadFromAsset(assetName)) {
        ShowErrorMessage("Model Load Error", "[LoadModel] Asset 読み込み失敗: " + assetName);
        m_resource.reset();
        return false;
    }
    if (!m_resource->BuildGPU(DirectX11::GetInstance()->GetDevice())) {
        ShowErrorMessage("Model GPU Error", "[LoadModel] GPU バッファ作成失敗: " + assetName);
        m_resource.reset();
        return false;
    }

    m_animCtrl.SetResource(m_resource);
    if (!m_resource->animations.empty()) {
        if (!m_animCtrl.Play(0, true, 1.0)) {
            ShowErrorMessage("Animation Error", "[LoadModel] 最初のアニメーション再生に失敗");
        }
        else {
            m_selectedClip = 0;
        }
    }
    SetUnifiedShader(m_unifiedVS, m_unifiedPS);
    if (!ResolveAndLoadMaterialTextures()) {
        ShowErrorMessage("Material Load Warning", "[LoadModel] マテリアルテクスチャ解決の一部に失敗");
    }
    ClassifySubMeshes();
    m_loaded = true;
    m_currentAsset = assetName;

    // 選択保持
    RefreshCachedModelAssetList(true);
    for (int i = 0; i < (int)m_cachedModelAssets.size(); ++i) {
        if (m_cachedModelAssets[i] == assetName) {
            m_cachedAssetSelected = i;
            m_selectedCachedAssetName = assetName;
            break;
        }
    }
    return true;
}

void AdvancedModelComponent::SetUnifiedShader(const std::string& vs, const std::string& ps) {
    m_unifiedVS = vs;
    m_unifiedPS = ps;
    if (!m_resource) return;
    for (auto& m : m_resource->materials) {
        if (!m.overrideUnified) {
            m.vs = vs;
            m.ps = ps;
        }
    }
    EnsureInputLayout(vs);
}

void AdvancedModelComponent::EnsureInputLayout(const std::string& vs) {
    if (vs.empty()) return;
    const void* bc = nullptr; size_t sz = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(vs, &bc, &sz)) {
        ShowErrorMessage("Shader Error", "[EnsureInputLayout] VS Bytecode 未取得: " + vs);
        return;
    }
    auto dev = DirectX11::GetInstance()->GetDevice();
    D3D11_INPUT_ELEMENT_DESC desc[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,(UINT)offsetof(AModelVertex,position),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,(UINT)offsetof(AModelVertex,normal),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(AModelVertex,tangent),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,(UINT)offsetof(AModelVertex,uv),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEINDICES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,(UINT)offsetof(AModelVertex,boneIndices),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEWEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(AModelVertex,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    m_inputLayout.Reset();
    if (FAILED(dev->CreateInputLayout(desc, _countof(desc), bc, sz, m_inputLayout.GetAddressOf()))) {
        ShowErrorMessage("Shader Error", "[EnsureInputLayout] CreateInputLayout 失敗");
    }
}

void AdvancedModelComponent::EditUpdate() {
    static auto prev = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    double dt = std::chrono::duration<double>(now - prev).count();
    prev = now;
    UpdateAnimation(dt);

    // 5 秒ごとの自動リスト再構築（選択保持対応）
    double tSec = std::chrono::duration<double>(now.time_since_epoch()).count();
    if (tSec - m_lastRefreshTime > 5.0) {
        RefreshCachedModelAssetList(false); // force=false で選択保持
        m_lastRefreshTime = tSec;
    }
}

void AdvancedModelComponent::InGameUpdate() {
    static auto prev = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    double dt = std::chrono::duration<double>(now - prev).count();
    prev = now;
    UpdateAnimation(dt);
}

void AdvancedModelComponent::UpdateAnimation(double dt) {
    if (!m_enableSkinning || !m_resource) return;
    m_animCtrl.Update(dt);
    UpdateBoneCBuffer();
}

void AdvancedModelComponent::UpdateBoneCBuffer() {
    if (!m_resource || !m_enableSkinning) return;
    ShaderManager::GetInstance()->SetCBufferVariable(
        ShaderStage::VS, m_unifiedVS, "SkinCB", "gBones",
        m_resource->finalBonePalette, (UINT)(sizeof(DirectX::XMMATRIX) * AMODEL_MAX_BONES)
    );
    ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, m_unifiedVS);
}

bool AdvancedModelComponent::PlayAnimation(const std::string& clipName, bool loop, double speed) {
    if (!m_resource) return false;
    for (int i = 0; i < (int)m_resource->animations.size(); i++) {
        if (m_resource->animations[i].name == clipName) {
            if (!m_animCtrl.Play(i, loop, speed)) {
                ShowErrorMessage("Animation Error", "[PlayAnimation] 再生に失敗: " + clipName);
                return false;
            }
            m_selectedClip = i;
            return true;
        }
    }
    ShowErrorMessage("Animation Error", "[PlayAnimation] クリップが存在しません: " + clipName);
    return false;
}

bool AdvancedModelComponent::BlendToAnimation(const std::string& clipName, double duration, AnimBlendMode mode, bool loop, double speed) {
    if (!m_resource) return false;
    for (int i = 0; i < (int)m_resource->animations.size(); i++) {
        if (m_resource->animations[i].name == clipName) {
            if (!m_animCtrl.BlendTo(i, duration, mode, loop, speed)) {
                ShowErrorMessage("Animation Error", "[BlendToAnimation] ブレンド開始失敗: " + clipName);
                return false;
            }
            return true;
        }
    }
    ShowErrorMessage("Animation Error", "[BlendToAnimation] クリップが存在しません: " + clipName);
    return false;
}

void AdvancedModelComponent::TouchCacheOrder(const std::string& key) {
    auto it = std::find(m_cacheOrder.begin(), m_cacheOrder.end(), key);
    if (it != m_cacheOrder.end()) {
        m_cacheOrder.erase(it);
    }
    m_cacheOrder.push_back(key);
    if (m_cacheOrder.size() > TEXCACHE_LIMIT) {
        std::string old = m_cacheOrder.front();
        m_cacheOrder.pop_front();
        auto it2 = m_texCache.find(old);
        if (it2 != m_texCache.end()) m_texCache.erase(it2);
    }
}

bool AdvancedModelComponent::SetMaterialTexture(uint32_t matIdx, uint32_t slot, const std::string& assetName) {
    if (!m_resource) return false;
    if (matIdx >= m_resource->materials.size()) {
        ShowErrorMessage("Material Error", "[SetMaterialTexture] material index 範囲外");
        return false;
    }
    auto& mat = m_resource->materials[matIdx];
    if (slot >= mat.textures.size()) mat.textures.resize(slot + 1);

    if (auto it = m_texCache.find(assetName); it != m_texCache.end()) {
        mat.textures[slot].srv = it->second;
        mat.textures[slot].assetName = assetName;
        TouchCacheOrder(assetName);
        return true;
    }

    std::vector<uint8_t> data;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, data)) {
        ShowErrorMessage("Material Error", "[SetMaterialTexture] テクスチャ資産取得失敗: " + assetName);
        return false;
    }

    DirectX::ScratchImage img;
    std::string ext;
    if (auto p = assetName.find_last_of('.'); p != std::string::npos) {
        ext = assetName.substr(p + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    HRESULT hr;
    if (ext == "dds")
        hr = LoadFromDDSMemory(data.data(), data.size(), DDS_FLAGS_NONE, nullptr, img);
    else
        hr = LoadFromWICMemory(data.data(), data.size(), WIC_FLAGS_FORCE_SRGB, nullptr, img);

    if (FAILED(hr)) {
        ShowErrorMessage("Material Error", "[SetMaterialTexture] デコード失敗: " + assetName);
        return false;
    }
    if (img.GetMetadata().mipLevels <= 1) {
        ScratchImage mip;
        if (SUCCEEDED(GenerateMipMaps(img.GetImages(), img.GetImageCount(), img.GetMetadata(),
            TEX_FILTER_DEFAULT, 0, mip))) {
            img = std::move(mip);
        }
    }
    auto dev = DirectX11::GetInstance()->GetDevice();
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (FAILED(CreateShaderResourceView(dev, img.GetImages(), img.GetImageCount(),
        img.GetMetadata(), srv.GetAddressOf()))) {
        ShowErrorMessage("Material Error", "[SetMaterialTexture] CreateSRV 失敗: " + assetName);
        return false;
    }
    m_texCache[assetName] = srv;
    TouchCacheOrder(assetName);

    mat.textures[slot].srv = srv;
    mat.textures[slot].assetName = assetName;
    return true;
}

bool AdvancedModelComponent::ResolveAndLoadMaterialTextures() {
    if (!m_resource) return false;
    auto all = AssetsManager::GetInstance()->ListAssets();
    for (uint32_t mi = 0; mi < m_resource->materials.size(); mi++) {
        auto& mat = m_resource->materials[mi];
        for (size_t s = 0; s < mat.textures.size(); s++) {
            auto& ts = mat.textures[s];
            if (ts.originalPath.empty()) continue;
            if (ts.srv) continue;
            std::string fileOnly = ts.originalPath;
            if (auto p = fileOnly.find_last_of("/\\"); p != std::string::npos)
                fileOnly = fileOnly.substr(p + 1);
            std::string lowerTarget = fileOnly;
            std::transform(lowerTarget.begin(), lowerTarget.end(), lowerTarget.begin(), ::tolower);

            std::string resolved;
            for (auto& a : all) {
                std::string f = a;
                if (auto q = f.find_last_of("/\\"); q != std::string::npos)
                    f = f.substr(q + 1);
                std::string lf = f;
                std::transform(lf.begin(), lf.end(), lf.begin(), ::tolower);
                if (lf == lowerTarget) { resolved = a; break; }
            }
            if (!resolved.empty()) {
                SetMaterialTexture(mi, (uint32_t)s, resolved);
                ts.fileName = fileOnly;
            }
        }
    }
    return true;
}

MeshPartType AdvancedModelComponent::GuessPart(const std::string& n) {
    std::string low = n;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    if (low.find("head") != std::string::npos || low.find("face") != std::string::npos || low.find("hair") != std::string::npos) return MeshPartType::HEAD;
    if (low.find("body") != std::string::npos || low.find("spine") != std::string::npos || low.find("torso") != std::string::npos) return MeshPartType::BODY;
    if ((low.find("arm") != std::string::npos || low.find("hand") != std::string::npos) && (low.find("left") != std::string::npos || low.find("_l") != std::string::npos)) return MeshPartType::ARM_LEFT;
    if ((low.find("arm") != std::string::npos || low.find("hand") != std::string::npos) && (low.find("right") != std::string::npos || low.find("_r") != std::string::npos)) return MeshPartType::ARM_RIGHT;
    if ((low.find("leg") != std::string::npos || low.find("foot") != std::string::npos) && (low.find("left") != std::string::npos || low.find("_l") != std::string::npos)) return MeshPartType::LEG_LEFT;
    if ((low.find("leg") != std::string::npos || low.find("foot") != std::string::npos) && (low.find("right") != std::string::npos || low.find("_r") != std::string::npos)) return MeshPartType::LEG_RIGHT;
    if (low.find("weapon") != std::string::npos || low.find("sword") != std::string::npos) return MeshPartType::WEAPON;
    if (low.find("accessory") != std::string::npos || low.find("ring") != std::string::npos) return MeshPartType::ACCESSORY;
    return MeshPartType::OTHER;
}

void AdvancedModelComponent::ClassifySubMeshes() {
    m_submeshPartMap.clear();
    if (!m_resource) return;
    for (auto& sm : m_resource->submeshes) {
        m_submeshPartMap.push_back(GuessPart(sm.name));
    }
}

bool AdvancedModelComponent::IsModelAsset(const std::string& name) const {
    return CaseInsensitiveEndsWith(name, ".fbx")
        || CaseInsensitiveEndsWith(name, ".obj")
        || CaseInsensitiveEndsWith(name, ".gltf")
        || CaseInsensitiveEndsWith(name, ".glb");
}

void AdvancedModelComponent::RefreshCachedModelAssetList(bool force) {
    auto cached = AssetsManager::GetInstance()->ListCachedAssets();
    std::vector<std::string> filtered;
    std::string lfilter = m_modelFilter;
    std::transform(lfilter.begin(), lfilter.end(), lfilter.begin(), ::tolower);

    for (auto& a : cached) {
        if (!IsModelAsset(a)) continue;
        if (!lfilter.empty()) {
            std::string low = a;
            std::transform(low.begin(), low.end(), low.begin(), ::tolower);
            if (low.find(lfilter) == std::string::npos) continue;
        }
        filtered.push_back(a);
    }
    std::sort(filtered.begin(), filtered.end());

    // リスト変化判定（force またはサイズ/要素差異）
    bool changed = force || (filtered.size() != m_cachedModelAssets.size());
    if (!changed) {
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (filtered[i] != m_cachedModelAssets[i]) { changed = true; break; }
        }
    }
    if (!changed) return; // 変化なしなら何もしない

    m_cachedModelAssets = std::move(filtered);

    // 選択保持処理
    int newIndex = -1;
    if (!m_selectedCachedAssetName.empty()) {
        for (int i = 0; i < (int)m_cachedModelAssets.size(); ++i) {
            if (m_cachedModelAssets[i] == m_selectedCachedAssetName) {
                newIndex = i;
                break;
            }
        }
    }
    // 新規ロード直後は m_currentAsset を優先
    if (newIndex == -1 && !m_currentAsset.empty()) {
        for (int i = 0; i < (int)m_cachedModelAssets.size(); ++i) {
            if (m_cachedModelAssets[i] == m_currentAsset) {
                newIndex = i;
                m_selectedCachedAssetName = m_currentAsset;
                break;
            }
        }
    }
    // 見つからなかった場合は従来インデックスを維持（存在しないなら自然に外れる）
    if (newIndex != -1) {
        m_cachedAssetSelected = newIndex;
    }
}

void AdvancedModelComponent::Draw() {
    if (!m_loaded || !m_resource) return;
    auto ctx = DirectX11::GetInstance()->GetContext();
    if (!ctx) return;

    UINT stride = sizeof(AModelVertex), offset = 0;
    ID3D11Buffer* vb = m_resource->vb.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_resource->ib.Get(), DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetInputLayout(m_inputLayout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    XMMATRIX world = XMMatrixIdentity();
    if (_Parent) {
        Transform tr = _Parent->GetTransform();
        XMMATRIX S = XMMatrixScaling(tr.scale.x, tr.scale.y, tr.scale.z);
        XMMATRIX R = XMMatrixRotationRollPitchYaw(tr.rotation.x, tr.rotation.y, tr.rotation.z);
        XMMATRIX T = XMMatrixTranslation(tr.position.x, tr.position.y, tr.position.z);
        world = S * R * T;
    }

    CameraComponent* cam = nullptr;
    if (_Parent && _Parent->GetParentScene())
        cam = _Parent->GetParentScene()->GetMainCamera();
    if (!cam) return;

    XMFLOAT4X4 wT;
    XMStoreFloat4x4(&wT, XMMatrixTranspose(world));
    auto viewT = cam->GetViewMatrix(true);
    auto projT = cam->GetProjectionMatrix(true);

    for (size_t i = 0; i < m_resource->submeshes.size(); i++) {
        if (i < m_submeshPartMap.size()) {
            if (!m_partVisible[m_submeshPartMap[i]]) continue;
        }
        auto& sm = m_resource->submeshes[i];
        if (sm.materialIndex >= m_resource->materials.size()) continue;
        auto& mat = m_resource->materials[sm.materialIndex];

        auto* vs = ShaderManager::GetInstance()->GetVertexShader(mat.vs);
        auto* ps = ShaderManager::GetInstance()->GetPixelShader(mat.ps);
        if (!vs || !ps) continue;

        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vs, "CameraCB", "world", &wT, sizeof(wT));
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vs, "CameraCB", "view", &viewT, sizeof(viewT));
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vs, "CameraCB", "proj", &projT, sizeof(projT));
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, mat.vs);

        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.ps, "MaterialCB", "BaseColor", &mat.baseColor, sizeof(mat.baseColor));
        struct MR { float Metallic; float Rough; DirectX::XMFLOAT2 pad; };
        MR mr{ mat.metallic, mat.roughness,{0,0} };
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.ps, "MaterialCB", "MetallicRoughness", &mr, sizeof(mr));
        UINT flag = (!mat.textures.empty() && mat.textures[0].srv) ? 1u : 0u;
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.ps, "MaterialCB", "UseBaseTex", &flag, sizeof(flag));
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::PS, mat.ps);

        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);

        if (!mat.textures.empty()) {
            ID3D11ShaderResourceView* arr[8]{};
            UINT cnt = 0;
            for (auto& ts : mat.textures) {
                if (cnt >= 8) break;
                arr[cnt++] = ts.srv.Get();
            }
            if (cnt > 0) ctx->PSSetShaderResources(0, cnt, arr);
        }

        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }
}

void AdvancedModelComponent::DrawInspector() {
    std::string id = "##AdvModel" + std::to_string((uintptr_t)this);
    if (!ImGui::CollapsingHeader(("AdvancedModelComponent" + id).c_str())) return;

    DrawSection_Model();
    DrawSection_Shader();
    DrawSection_Materials();
    DrawSection_Animation();
    DrawSection_Parts();
    DrawSection_Info();
}

void AdvancedModelComponent::DrawSection_Model() {
    if (!ImGui::CollapsingHeader("Model")) return;

    ImGui::TextUnformatted("[Cached Model Assets]");
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputText("Filter", m_modelFilter, sizeof(m_modelFilter))) {
        RefreshCachedModelAssetList(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        RefreshCachedModelAssetList(true);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Filter")) {
        m_modelFilter[0] = '\0';
        RefreshCachedModelAssetList(true);
    }

    if (m_cachedModelAssets.empty()) {
        if (ImGui::Button("ListCached")) {
            RefreshCachedModelAssetList(true);
        }
        ImGui::TextUnformatted("キャッシュ済みモデルなし / AssetsManager::LoadAsset で読み込んでください");
    }
    else {
        std::string currentLabel = (m_cachedAssetSelected >= 0 && m_cachedAssetSelected < (int)m_cachedModelAssets.size()) ?
            m_cachedModelAssets[m_cachedAssetSelected] : "<select>";
        if (ImGui::BeginCombo("CachedModels", currentLabel.c_str())) {
            for (int i = 0; i < (int)m_cachedModelAssets.size(); ++i) {
                bool sel = (i == m_cachedAssetSelected);
                if (ImGui::Selectable(m_cachedModelAssets[i].c_str(), sel)) {
                    m_cachedAssetSelected = i;
                    if (i >= 0 && i < (int)m_cachedModelAssets.size())
                        m_selectedCachedAssetName = m_cachedModelAssets[i]; // 選択保持文字列更新
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        bool canLoad = (m_cachedAssetSelected >= 0 && m_cachedAssetSelected < (int)m_cachedModelAssets.size());
        if (!canLoad) ImGui::BeginDisabled();
        if (ImGui::Button("Load Selected") && canLoad) {
            if (!LoadModel(m_cachedModelAssets[m_cachedAssetSelected], true)) {
                ShowErrorMessage("Model Load Error", "選択モデルのロードに失敗しました。");
            }
        }
        if (!canLoad) ImGui::EndDisabled();
    }

    ImGui::SeparatorText("Manual Path");
    ImGui::InputText("Asset", m_modelPathInput, sizeof(m_modelPathInput));
    ImGui::SameLine();
    if (ImGui::Button("Load Path")) {
        if (m_modelPathInput[0]) {
            if (!LoadModel(m_modelPathInput, true)) {
                ShowErrorMessage("Model Load Error", "手動指定ロード失敗: " + std::string(m_modelPathInput));
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (!m_currentAsset.empty()) {
            if (!LoadModel(m_currentAsset, true)) {
                ShowErrorMessage("Model Reload Error", "リロード失敗: " + m_currentAsset);
            }
        }
    }
    ImGui::Checkbox("Skinning", &m_enableSkinning);

    if (m_loaded) {
        ImGui::Text("Current: %s", m_currentAsset.c_str());
        ImGui::Text("SubMeshes: %zu  Mats:%zu  Bones:%zu  Clips:%zu",
            m_resource->submeshes.size(), m_resource->materials.size(),
            m_resource->bones.size(), m_resource->animations.size());
    }
    else {
        ImGui::TextUnformatted("Not Loaded");
    }
}

void AdvancedModelComponent::DrawSection_Shader() {
    if (!ImGui::CollapsingHeader("Shader")) return;
    auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
    auto psList = ShaderManager::GetInstance()->GetShaderList("PS");

    static int vsSel = -1, psSel = -1;
    if (vsSel < 0) {
        for (int i = 0; i < (int)vsList.size(); i++) if (vsList[i] == m_unifiedVS) vsSel = i;
    }
    if (psSel < 0) {
        for (int i = 0; i < (int)psList.size(); i++) if (psList[i] == m_unifiedPS) psSel = i;
    }

    if (ImGui::BeginCombo("Unified VS", (vsSel >= 0) ? vsList[vsSel].c_str() : "-")) {
        for (int i = 0; i < (int)vsList.size(); i++) {
            bool sel = (i == vsSel);
            if (ImGui::Selectable(vsList[i].c_str(), sel)) vsSel = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::BeginCombo("Unified PS", (psSel >= 0) ? psList[psSel].c_str() : "-")) {
        for (int i = 0; i < (int)psList.size(); i++) {
            bool sel = (i == psSel);
            if (ImGui::Selectable(psList[i].c_str(), sel)) psSel = i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Apply Unified") && vsSel >= 0 && psSel >= 0) {
        SetUnifiedShader(vsList[vsSel], psList[psSel]);
    }
}

void AdvancedModelComponent::DrawSection_Materials() {
    if (!ImGui::CollapsingHeader("Materials")) return;
    if (!m_resource) { ImGui::TextUnformatted("No Resource"); return; }
    if (m_resource->materials.empty()) { ImGui::TextUnformatted("No Materials"); return; }

    ImGui::DragInt("Material Index", &m_selectedMaterial, 1, 0, (int)m_resource->materials.size() - 1);
    m_selectedMaterial = std::clamp(m_selectedMaterial, 0, (int)m_resource->materials.size() - 1);
    auto& mat = m_resource->materials[m_selectedMaterial];

    ImGui::Checkbox("Override", &mat.overrideUnified);
    ImGui::ColorEdit4("BaseColor", &mat.baseColor.x);
    ImGui::DragFloat("Metallic", &mat.metallic, 0.01f, 0.f, 1.f);
    ImGui::DragFloat("Roughness", &mat.roughness, 0.01f, 0.f, 1.f);

    if (ImGui::Button("Add Tex Slot")) {
        mat.textures.resize(mat.textures.size() + 1);
    }
    for (size_t i = 0; i < mat.textures.size(); i++) {
        auto& ts = mat.textures[i];
        ImGui::PushID((int)i);
        if (ImGui::TreeNode(("Slot " + std::to_string(i)).c_str())) {
            ImGui::Text("Asset: %s", ts.assetName.c_str());
            static char input[260];
            if (ImGui::InputText("NewAsset", input, sizeof(input), ImGuiInputTextFlags_EnterReturnsTrue)) {
                if (input[0]) SetMaterialTexture((uint32_t)m_selectedMaterial, (uint32_t)i, input);
            }
            if (ImGui::Button("LoadInput")) {
                if (input[0]) {
                    if (!SetMaterialTexture((uint32_t)m_selectedMaterial, (uint32_t)i, input)) {
                        ShowErrorMessage("Material Error", "テクスチャ読み込み失敗: " + std::string(input));
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                ts.srv.Reset(); ts.assetName.clear();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void AdvancedModelComponent::DrawSection_Animation() {
    if (!ImGui::CollapsingHeader("Animation")) return;
    if (!m_resource || m_resource->animations.empty()) {
        ImGui::TextUnformatted("No Clips");
        return;
    }
    if (ImGui::BeginCombo("Clip",
        (m_selectedClip >= 0 && m_selectedClip < (int)m_resource->animations.size()) ?
        m_resource->animations[m_selectedClip].name.c_str() : "-")) {
        for (int i = 0; i < (int)m_resource->animations.size(); i++) {
            bool sel = (i == m_selectedClip);
            if (ImGui::Selectable(m_resource->animations[i].name.c_str(), sel)) {
                m_selectedClip = i;
                PlayAnimation(m_resource->animations[i].name, true, 1.0);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    static char blendTarget[128] = "";
    static float blendDur = 0.3f;
    static int blendMode = (int)AnimBlendMode::Linear;
    ImGui::InputText("BlendTarget", blendTarget, sizeof(blendTarget));
    ImGui::DragFloat("BlendDuration", &blendDur, 0.01f, 0.0f, 5.0f);
    const char* modes[] = { "None","Linear","PositionDelta" };
    ImGui::Combo("BlendMode", &blendMode, modes, IM_ARRAYSIZE(modes));
    if (ImGui::Button("BlendTo") && blendTarget[0]) {
        BlendToAnimation(blendTarget, blendDur, (AnimBlendMode)blendMode, true, 1.0);
    }
}

void AdvancedModelComponent::DrawSection_Parts() {
    if (!ImGui::CollapsingHeader("Parts")) return;
    const char* names[] = { "Head","Body","ArmL","ArmR","LegL","LegR","Weapon","Accessory","Other" };
    for (int i = 0; i < 9; i++) {
        auto pt = (MeshPartType)i;
        bool v = m_partVisible[pt];
        if (ImGui::Checkbox(names[i], &v)) {
            m_partVisible[pt] = v;
        }
    }
}

void AdvancedModelComponent::DrawSection_Info() {
    if (!ImGui::CollapsingHeader("Info")) return;
    if (!m_resource) return;
    ImGui::Text("Asset: %s", m_currentAsset.c_str());
    ImGui::Text("Vertices: %zu  Indices:%zu", m_resource->vertices.size(), m_resource->indices.size());
    ImGui::Text("SubMeshes: %zu  Materials:%zu", m_resource->submeshes.size(), m_resource->materials.size());
    ImGui::Text("Bones: %zu  AnimClips:%zu", m_resource->bones.size(), m_resource->animations.size());
}