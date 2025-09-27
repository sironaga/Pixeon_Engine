#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <assimp/scene.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

#if _MSC_VER >= 1930
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#elif _MSC_VER >= 1920
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc142-mtd.lib")
#else
#pragma comment(lib, "assimp-vc142-mt.lib")
#endif
#elif _MSC_VER >= 1910
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc141-mtd.lib")
#else
#pragma comment(lib, "assimp-vc141-mt.lib")
#endif
#endif


#include "AssetsManager.h" 

constexpr uint32_t AMODEL_MAX_BONES = 512;
constexpr uint32_t AMODEL_MAX_INFLUENCES = 4;

struct AModelVertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT2 uv;
    uint32_t boneIndices[AMODEL_MAX_INFLUENCES]{ 0,0,0,0 };
    float    boneWeights[AMODEL_MAX_INFLUENCES]{ 0,0,0,0 };
};

struct AModelMaterial {
    DirectX::XMFLOAT4 baseColor{ 1,1,1,1 };
    float metallic = 0.0f;
    float roughness = 0.8f;
    std::string vs;
    std::string ps;
    bool overrideUnified = false;
    struct TexSlot {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        std::string assetName;
        std::string originalPath;
        std::string fileName;
    };
    std::vector<TexSlot> textures;
};

struct AModelSubMesh {
    uint32_t materialIndex = 0;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    uint32_t vertexOffset = 0;
    bool     skinned = false;
    std::string name;
};

struct AModelBone {
    std::string name;
    int parent = -1;
    DirectX::XMMATRIX offset;
    DirectX::XMMATRIX local;
    DirectX::XMMATRIX global;
};

struct AModelKeyFrame {
    double time = 0.0;
    DirectX::XMFLOAT3 t{ 0,0,0 };
    DirectX::XMFLOAT4 r{ 0,0,0,1 };
    DirectX::XMFLOAT3 s{ 1,1,1 };
};
struct AModelBoneChannel {
    std::string boneName;
    std::vector<AModelKeyFrame> translations;
    std::vector<AModelKeyFrame> rotations;
    std::vector<AModelKeyFrame> scalings;
};
struct AModelAnimation {
    std::string name;
    double duration = 0.0;
    double ticksPerSecond = 25.0;
    std::vector<AModelBoneChannel> channels;
};

class ModelRuntimeResource {
public:
    bool LoadFromAsset(const std::string& assetName);
    bool BuildGPU(ID3D11Device* dev);
    void ReleaseGPU();

    int FindBoneIndex(const std::string& name) const;

    std::vector<AModelVertex>   vertices;
    std::vector<uint32_t>       indices;
    std::vector<AModelSubMesh>  submeshes;
    std::vector<AModelMaterial> materials;
    std::vector<AModelBone>     bones;
    std::vector<AModelAnimation> animations;

    DirectX::XMMATRIX finalBonePalette[AMODEL_MAX_BONES]{};

    Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ib;

    std::string unifiedVS = "VS_ModelSkin";
    std::string unifiedPS = "PS_ModelSkin";

    std::string sourceAssetName;
    bool hasSkin = false;

private:
    void ProcessNode(aiNode* node, const aiScene* scene, int parent);
    void ProcessMesh(aiMesh* mesh, const aiScene* scene);
    bool ImportScene(const std::string& assetName, const aiScene* scene);
    void ExtractMaterial(aiMaterial* aimat, uint32_t index, const std::string& baseDir);

    static void ExtractColors(aiMaterial* aimat, AModelMaterial& out);
    static DirectX::XMMATRIX ToXM(const aiMatrix4x4& m);
};