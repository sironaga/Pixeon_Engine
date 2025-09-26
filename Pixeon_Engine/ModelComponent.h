#pragma once
#include "Component.h"
#include "ModelRuntimeResource.h"
#include "ModelAnimationController.h"
#include "ShaderManager.h"
#include "AssetsManager.h"          // NEW: キャッシュ列挙に利用
#include <unordered_map>
#include <deque>

enum class MeshPartType {
    HEAD, BODY, ARM_LEFT, ARM_RIGHT, LEG_LEFT, LEG_RIGHT, WEAPON, ACCESSORY, OTHER
};

class AdvancedModelComponent : public Component {
public:
    AdvancedModelComponent();
    ~AdvancedModelComponent();

    void Init(Object* Prt) override;
    void UInit() override;
    void EditUpdate() override;
    void InGameUpdate() override;
    void Draw() override;
    void DrawInspector() override;

    bool LoadModel(const std::string& assetName, bool force = false);
    void SetUnifiedShader(const std::string& vs, const std::string& ps);

    bool PlayAnimation(const std::string& clipName, bool loop = true, double speed = 1.0);
    bool BlendToAnimation(const std::string& clipName, double duration, AnimBlendMode mode, bool loop = true, double speed = 1.0);

private:
    void Clear();
    void EnsureInputLayout(const std::string& vs);
    void UpdateAnimation(double dt);
    void UpdateBoneCBuffer();

    bool SetMaterialTexture(uint32_t matIdx, uint32_t slot, const std::string& assetName);
    bool ResolveAndLoadMaterialTextures();

    void ClassifySubMeshes();
    static MeshPartType GuessPart(const std::string& n);

    // Inspector sections
    void DrawSection_Model();
    void DrawSection_Shader();
    void DrawSection_Materials();
    void DrawSection_Animation();
    void DrawSection_Parts();
    void DrawSection_Info();

    // === NEW: メモリ展開済みモデル選択支援 ===
    void RefreshCachedModelAssetList(bool force = false);
    bool IsModelAsset(const std::string& name) const;

private:
    std::shared_ptr<ModelRuntimeResource> m_resource;
    ModelAnimationController m_animCtrl;

    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    std::string m_currentAsset;
    bool m_loaded = false;
    bool m_enableSkinning = true;

    std::string m_unifiedVS;
    std::string m_unifiedPS;

    std::unordered_map<MeshPartType, bool> m_partVisible;
    std::vector<MeshPartType> m_submeshPartMap;

    int  m_selectedClip = -1;
    char m_modelPathInput[260]{};
    int  m_selectedMaterial = 0;

    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_texCache;
    std::deque<std::string> m_cacheOrder;
    void TouchCacheOrder(const std::string& key);

    // === NEW: モデルキャッシュ UI 状態 ===
    std::vector<std::string> m_cachedModelAssets;  // メモリ展開済み（ListCachedAssets）からフィルタ
    int  m_cachedAssetSelected = -1;
    char m_modelFilter[64]{};                      // 部分一致フィルタ
    double m_lastRefreshTime = 0.0;                // 自動リフレッシュ間隔制御用（任意）
};