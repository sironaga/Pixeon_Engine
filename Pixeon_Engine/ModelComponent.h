#pragma once
#include "Component.h"
#include "AssetsManager.h"
#include "ShaderManager.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

constexpr uint32_t MODEL_MAX_BONES = 512;
constexpr uint32_t MODEL_MAX_INFLUENCES = 4;
constexpr size_t   MODEL_TEXCACHE_LIMIT = 256; // 簡易 LRU 上限

struct ModelVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv;
    uint32_t boneIndices[MODEL_MAX_INFLUENCES] = { 0,0,0,0 };
    float    boneWeights[MODEL_MAX_INFLUENCES] = { 0,0,0,0 };
};

struct ModelMaterial {
    std::string vertexShader;
    std::string pixelShader;
    bool        overrideUnified = false;
    DirectX::XMFLOAT4 baseColor = { 1,1,1,1 };
    float metallic = 0.0f;
    float roughness = 0.8f;
    struct TextureSlot {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        std::string assetName;
    };
    std::vector<TextureSlot> textures;
};

struct SubMesh {
    uint32_t materialIndex = 0;
    uint32_t indexCount = 0;
    uint32_t indexOffset = 0;
    uint32_t vertexOffset = 0;
    bool     skinned = false;
};

struct Bone {
    std::string name;
    int parent = -1;
    DirectX::XMMATRIX offset;
    DirectX::XMMATRIX local;
    DirectX::XMMATRIX global;
};

struct BoneKeyFrame {
    double time = 0.0;
    DirectX::XMFLOAT3 t;
    DirectX::XMFLOAT4 r;
    DirectX::XMFLOAT3 s;
};
struct BoneAnimChannel {
    std::string boneName;
    std::vector<BoneKeyFrame> translations;
    std::vector<BoneKeyFrame> rotations;
    std::vector<BoneKeyFrame> scalings;
};
struct AnimationClip {
    std::string name;
    double duration = 0.0;
    double ticksPerSecond = 25.0;
    std::vector<BoneAnimChannel> channels;
};

class ModelComponent : public Component {
public:
    ModelComponent();
    ~ModelComponent();

    void Init(Object* Prt) override;
    void Draw() override;
    void EditUpdate() override;
    void InGameUpdate() override;
    void UInit() override;
    void DrawInspector() override;

    bool LoadModelFromAsset(const std::string& assetName, bool forceReload = false);

    void SetUnifiedShader(const std::string& vs, const std::string& ps);
    void SetMaterialShader(uint32_t materialIndex, const std::string& vs, const std::string& ps, bool setOverride = true);
    bool SetMaterialTexture(uint32_t materialIndex, uint32_t slot, const std::string& assetName);

    void PlayAnimation(const std::string& clipName, bool loop = true, float speed = 1.0f);
    void StopAnimation();
    void SetAnimationTime(double time);
    void EnableSkinning(bool e) { m_enableSkinning = e; }

    void SolveIK_CCD(const std::vector<int>& chain, const DirectX::XMFLOAT3& targetWorld, int iterations, float thresholdDeg = 0.5f);

private:
    void ClearModel();
    bool ImportAssimpScene(const std::string& assetName, const aiScene* scene);
    void ProcessNode(aiNode* node, const aiScene* scene, int parentIndex);
    void ProcessMesh(const aiMesh* mesh, const aiScene* scene);

    void BuildBuffers();
    void UpdateAnimation(double deltaTime);
    void ApplyAnimationToBones(double timeInTicks);
    void UpdateBoneMatrices();
    int  FindBoneIndex(const std::string& name) const;
    void EnsureInputLayout(const std::string& vsName);

    static void InterpolateChannel(const BoneAnimChannel& ch, double time,
        DirectX::XMFLOAT3& outT,
        DirectX::XMFLOAT4& outR,
        DirectX::XMFLOAT3& outS);

    bool LoadTextureFromAsset(const std::string& assetName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV);
    static bool IsModelAssetName(const std::string& name);

    std::string NormalizeTexturePath(const std::string& raw, const std::string& modelDir);
    void ExtractMaterialColors(aiMaterial* aimat, ModelMaterial& outMat);

    void EnsureFallbackWhiteTexture();
    void InsertTextureCache(const std::string& key, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv);

private:
    Microsoft::WRL::ComPtr<ID3D11Buffer>      m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer>      m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    std::vector<ModelVertex>   m_vertices;
    std::vector<uint32_t>      m_indices;
    std::vector<SubMesh>       m_subMeshes;
    std::vector<ModelMaterial> m_materials;

    std::vector<Bone>                   m_bones;
    std::unordered_map<std::string, int> m_boneNameToIndex;
    DirectX::XMMATRIX                   m_finalBoneMatrices[MODEL_MAX_BONES]{};

    std::vector<AnimationClip> m_clips;
    int     m_currentClip = -1;
    bool    m_loop = true;
    double  m_animTime = 0.0;
    double  m_animSpeed = 1.0;

    bool    m_modelLoaded = false;
    bool    m_enableSkinning = true;

    std::string m_unifiedVS;
    std::string m_unifiedPS;

    // Inspector
    char m_modelAssetInput[260] = "";
    char m_modelSearch[64] = "";
    char m_newTextureName[260] = "";
    int  m_selectedAnimIndex = -1;
    int  m_selectedMaterial = 0;
    std::string m_currentModelAsset;

    // IK
    char m_ikChainInput[128] = "";
    DirectX::XMFLOAT3 m_ikTarget{ 0,0,0 };
    int   m_ikIterations = 5;
    float m_ikThresholdDeg = 0.5f;

    // テクスチャキャッシュ (LRU 保持)
    std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> m_textureCache;
    std::deque<std::string> m_cacheOrder;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fallbackWhite; // フォールバック用
};