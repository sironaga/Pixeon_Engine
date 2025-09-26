#include "ModelComponent.h"
#include "SettingManager.h"
#include "DirectXTex/DirectXTex.h"
#include "Object.h"
#include "Struct.h"
#include "EditrGUI.h"
#include "AssetsManager.h"
#include "System.h"
#include "CameraComponent.h"
#include "Scene.h"
#include <chrono>
#include <algorithm>
#include <cassert>
#include <sstream>

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

using namespace DirectX;

// ---------- ユーティリティ ----------
static XMMATRIX AiToXM(const aiMatrix4x4& m) {
    return XMMATRIX(
        (float)m.a1, (float)m.b1, (float)m.c1, (float)m.d1,
        (float)m.a2, (float)m.b2, (float)m.c2, (float)m.d2,
        (float)m.a3, (float)m.b3, (float)m.c3, (float)m.d3,
        (float)m.a4, (float)m.b4, (float)m.c4, (float)m.d4
    );
}

bool ModelComponent::IsModelAssetName(const std::string& name) {
    auto pos = name.find_last_of('.');
    if (pos == std::string::npos) return false;
    std::string ext = name.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == "fbx" || ext == "obj" || ext == "gltf" || ext == "glb" || ext == "dae");
}

// ---------- コンストラクタ ----------
ModelComponent::ModelComponent() {
    _ComponentName = "ModelComponent";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
    m_modelAssetInput[0] = '\0';
    m_modelSearch[0] = '\0';
    m_newTextureName[0] = '\0';
    m_ikChainInput[0] = '\0';
}

ModelComponent::~ModelComponent() {
    ClearModel();
}

// ---------- ライフサイクル ----------
void ModelComponent::Init(Object* Prt) { _Parent = Prt; }
void ModelComponent::UInit() { ClearModel(); }

void ModelComponent::ClearModel() {
    m_vertices.clear();
    m_indices.clear();
    m_subMeshes.clear();
    m_materials.clear();
    m_bones.clear();
    m_boneNameToIndex.clear();
    m_clips.clear();
    m_currentClip = -1;
    m_modelLoaded = false;
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();
    m_inputLayout.Reset();
    m_selectedAnimIndex = -1;
    m_selectedMaterial = 0;
    m_currentModelAsset.clear();
}

bool ModelComponent::LoadModelFromAsset(const std::string& assetName, bool forceReload) {
    if (m_modelLoaded && !forceReload && assetName == m_currentModelAsset)
        return true;

    ClearModel();

    std::vector<uint8_t> data;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, data)) {
        std::string dbg = "[ModelComponent] LoadModelFromAsset failed asset=" + assetName + "\n";
        OutputDebugStringA(dbg.c_str());
        return false;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(
        data.data(), data.size(),
        aiProcess_Triangulate |
        aiProcess_CalcTangentSpace |
        aiProcess_GenNormals |
        aiProcess_JoinIdenticalVertices |
        aiProcess_LimitBoneWeights |
        aiProcess_ImproveCacheLocality |
        aiProcess_SortByPType |
        aiProcess_GlobalScale
    );
    if (!scene || !scene->mRootNode) {
        OutputDebugStringA("[ModelComponent] Assimp ReadFileFromMemory failed\n");
        return false;
    }
    if (!ImportAssimpScene(assetName, scene)) {
        OutputDebugStringA("[ModelComponent] ImportAssimpScene failed\n");
        return false;
    }

    BuildBuffers();
    m_modelLoaded = true;
    m_currentModelAsset = assetName;

    m_unifiedVS = "VS_ModelSkin";
    m_unifiedPS = "PS_ModelPBR";
    SetUnifiedShader(m_unifiedVS, m_unifiedPS);

    if (!m_clips.empty()) {
        m_selectedAnimIndex = 0;
        m_currentClip = 0;
        PlayAnimation(m_clips[0].name, true, 1.0f);
    }
    return true;
}

// ---------- マテリアル情報 / テクスチャ取得ヘルパ ----------
void ModelComponent::GetMaterialColorFactors(aiMaterial* aimat, ModelMaterial& outMat)
{
    if (!aimat) return;
    aiColor4D col;
#ifdef AI_MATKEY_BASE_COLOR
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_BASE_COLOR, col)) {
        outMat.baseColor = XMFLOAT4(col.r, col.g, col.b, col.a);
    }
    else
#endif
        if (AI_SUCCESS == aimat->Get(AI_MATKEY_COLOR_DIFFUSE, col)) {
            outMat.baseColor = XMFLOAT4(col.r, col.g, col.b, col.a);
        }
#ifdef AI_MATKEY_METALLIC_FACTOR
    float metallic = 0.f;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_METALLIC_FACTOR, metallic))
        outMat.metallic = metallic;
#endif
#ifdef AI_MATKEY_ROUGHNESS_FACTOR
    float roughness = 0.f;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness))
        outMat.roughness = roughness;
#endif
}

std::string ModelComponent::NormalizeTexturePath(const std::string& rawPath, const std::string& modelDir)
{
    if (rawPath.empty()) return rawPath;
    if (rawPath[0] == '*') return rawPath; // 埋め込み

    std::string p = rawPath;
    std::replace(p.begin(), p.end(), '\\', '/');

    // 絶対パスならそのまま
    if (!p.empty()) {
        if (p[0] == '/' || (p.size() > 2 && p[1] == ':')) return p;
    }

    if (!modelDir.empty()) {
        if (p.rfind(modelDir + "/", 0) == 0) return p;
        return modelDir + "/" + p;
    }
    return p;
}

bool ModelComponent::TryLoadMaterialTexture(uint32_t matIndex, uint32_t slot, const std::string& texRelPath)
{
    if (texRelPath.empty()) return false;
    return SetMaterialTexture(matIndex, slot, texRelPath);
}

bool ModelComponent::CreateSRVFromRawRGBA(const uint8_t* pixels, uint32_t width, uint32_t height,
    DXGI_FORMAT fmt, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    if (!pixels || width == 0 || height == 0) return false;
    ID3D11Device* dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = fmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels;
    init.SysMemPitch = width * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = dev->CreateTexture2D(&desc, &init, tex.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = desc.Format;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    hr = dev->CreateShaderResourceView(tex.Get(), &srvd, outSRV.GetAddressOf());
    return SUCCEEDED(hr);
}

bool ModelComponent::CreateSRVFromCompressed(const uint8_t* data, size_t size, const char* formatHint,
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV)
{
    if (!data || size == 0) return false;
    ScratchImage img;
    HRESULT hr = E_FAIL;
    std::string hint = (formatHint ? formatHint : "");
    std::transform(hint.begin(), hint.end(), hint.begin(), ::tolower);

    if (hint == "dds")
        hr = LoadFromDDSMemory(data, size, DDS_FLAGS_NONE, nullptr, img);
    else if (hint == "tga")
        hr = LoadFromTGAMemory(data, size, nullptr, img);
    else
        hr = LoadFromWICMemory(data, size, WIC_FLAGS_FORCE_SRGB, nullptr, img);

    if (FAILED(hr)) return false;

    if (img.GetMetadata().mipLevels <= 1) {
        ScratchImage mip;
        if (SUCCEEDED(GenerateMipMaps(img.GetImages(), img.GetImageCount(),
            img.GetMetadata(), TEX_FILTER_DEFAULT, 0, mip)))
            img = std::move(mip);
    }

    hr = CreateShaderResourceView(DirectX11::GetInstance()->GetDevice(),
        img.GetImages(), img.GetImageCount(), img.GetMetadata(), outSRV.GetAddressOf());
    return SUCCEEDED(hr);
}

bool ModelComponent::TryLoadEmbeddedTexture(uint32_t matIndex, uint32_t slot, const aiTexture* aitex,
    const std::string& cacheKey, const char* formatHint)
{
    if (!aitex) return false;
    // キャッシュ
    auto it = m_textureCache.find(cacheKey);
    if (it != m_textureCache.end()) {
        if (slot >= m_materials[matIndex].textures.size())
            m_materials[matIndex].textures.resize(slot + 1);
        m_materials[matIndex].textures[slot].srv = it->second;
        m_materials[matIndex].textures[slot].assetName = cacheKey;
        return true;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (aitex->mHeight == 0) {
        // 圧縮
        if (!CreateSRVFromCompressed(reinterpret_cast<const uint8_t*>(aitex->pcData),
            aitex->mWidth, aitex->achFormatHint, srv))
        {
            OutputDebugStringA("[ModelComponent] Embedded compressed texture load failed\n");
            return false;
        }
    }
    else {
        // 非圧縮 (aiTexel は RGBA もしくは BGRA。Assimp ドキュメント参照。ここでは RGBA として扱う)
        std::vector<uint8_t> rgba;
        rgba.resize((size_t)aitex->mWidth * aitex->mHeight * 4);
        for (uint32_t i = 0; i < (uint32_t)(aitex->mWidth * aitex->mHeight); ++i) {
            const aiTexel& t = aitex->pcData[i];
            rgba[i * 4 + 0] = t.r;
            rgba[i * 4 + 1] = t.g;
            rgba[i * 4 + 2] = t.b;
            rgba[i * 4 + 3] = t.a;
        }
        if (!CreateSRVFromRawRGBA(rgba.data(), (uint32_t)aitex->mWidth, aitex->mHeight,
            DXGI_FORMAT_R8G8B8A8_UNORM, srv))
        {
            OutputDebugStringA("[ModelComponent] Embedded raw texture create failed\n");
            return false;
        }
    }

    m_textureCache[cacheKey] = srv;
    if (slot >= m_materials[matIndex].textures.size())
        m_materials[matIndex].textures.resize(slot + 1);
    m_materials[matIndex].textures[slot].srv = srv;
    m_materials[matIndex].textures[slot].assetName = cacheKey;
    return true;
}

bool ModelComponent::AcquireMaterialTextures(uint32_t matIndex, aiMaterial* aimat, const aiScene* scene, const std::string& modelDir)
{
    if (!aimat) return false;

    auto loadOne = [&](aiTextureType type, uint32_t slot, const char* tag) {
        if (aimat->GetTextureCount(type) > 0) {
            aiString path;
            if (AI_SUCCESS == aimat->GetTexture(type, 0, &path)) {
                std::string p = path.C_Str();
                if (!p.empty() && p[0] == '*') {
                    const aiTexture* aitex = scene->GetEmbeddedTexture(p.c_str());
                    std::string key = "embedded:" + std::to_string(matIndex) + ":" + p;
                    if (!TryLoadEmbeddedTexture(matIndex, slot, aitex, key, aitex ? aitex->achFormatHint : "")) {
                        std::string dbg = "[ModelComponent] Embedded texture load failed tag=" + std::string(tag) + "\n";
                        OutputDebugStringA(dbg.c_str());
                    }
                }
                else {
                    p = NormalizeTexturePath(p, modelDir);
                    if (!TryLoadMaterialTexture(matIndex, slot, p)) {
                        std::string dbg = "[ModelComponent] Texture load failed slot=" + std::to_string(slot) + " path=" + p + "\n";
                        OutputDebugStringA(dbg.c_str());
                    }
                }
            }
        }
        };

    // Diffuse / BaseColor
#ifdef AI_MATKEY_BASE_COLOR_TEXTURE
    if (aimat->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
        loadOne(aiTextureType_BASE_COLOR, 0, "BaseColor");
    else
#endif
        loadOne(aiTextureType_DIFFUSE, 0, "Diffuse");

    // Normal
    if (aimat->GetTextureCount(aiTextureType_NORMALS) > 0)
        loadOne(aiTextureType_NORMALS, 1, "Normal");
    else if (aimat->GetTextureCount(aiTextureType_HEIGHT) > 0)
        loadOne(aiTextureType_HEIGHT, 1, "HeightAsNormal");

#ifdef aiTextureType_METALNESS
    if (aimat->GetTextureCount(aiTextureType_METALNESS) > 0)
        loadOne(aiTextureType_METALNESS, 2, "Metalness");
#endif
#ifdef aiTextureType_DIFFUSE_ROUGHNESS
    if (aimat->GetTextureCount(aiTextureType_DIFFUSE_ROUGHNESS) > 0)
        loadOne(aiTextureType_DIFFUSE_ROUGHNESS, 2, "Roughness");
#endif

    loadOne(aiTextureType_EMISSIVE, 3, "Emissive");

    return true;
}

// ---------- インポート ----------
bool ModelComponent::ImportAssimpScene(const std::string& assetName, const aiScene* scene) {
    // モデルの相対ディレクトリ
    std::string modelDir;
    {
        auto pos = assetName.find_last_of("/\\");
        if (pos != std::string::npos) modelDir = assetName.substr(0, pos);
    }

    m_materials.resize(scene->mNumMaterials);
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aimat = scene->mMaterials[i];
        ModelMaterial mat;
        mat.baseColor = XMFLOAT4(1, 1, 1, 1);
        mat.metallic = 0.0f;
        mat.roughness = 0.8f;
        GetMaterialColorFactors(aimat, mat);
        m_materials[i] = std::move(mat);
    }
    // テクスチャ自動取得
    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        AcquireMaterialTextures(i, scene->mMaterials[i], scene, modelDir);
    }

    ProcessNode(scene->mRootNode, scene, -1);

    // アニメーション
    if (scene->mNumAnimations > 0) {
        m_clips.reserve(scene->mNumAnimations);
        for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
            aiAnimation* aiAnim = scene->mAnimations[i];
            AnimationClip clip;
            clip.name = aiAnim->mName.length ? aiAnim->mName.C_Str() : ("Anim" + std::to_string(i));
            clip.duration = aiAnim->mDuration;
            clip.ticksPerSecond = (aiAnim->mTicksPerSecond != 0.0) ? aiAnim->mTicksPerSecond : 25.0;
            for (uint32_t c = 0; c < aiAnim->mNumChannels; c++) {
                aiNodeAnim* ch = aiAnim->mChannels[c];
                BoneAnimChannel channel;
                channel.boneName = ch->mNodeName.C_Str();
                for (uint32_t k = 0; k < ch->mNumPositionKeys; k++) {
                    BoneKeyFrame kf; kf.time = ch->mPositionKeys[k].mTime;
                    kf.t = XMFLOAT3((float)ch->mPositionKeys[k].mValue.x,
                        (float)ch->mPositionKeys[k].mValue.y,
                        (float)ch->mPositionKeys[k].mValue.z);
                    channel.translations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumRotationKeys; k++) {
                    BoneKeyFrame kf; kf.time = ch->mRotationKeys[k].mTime;
                    kf.r = XMFLOAT4((float)ch->mRotationKeys[k].mValue.x,
                        (float)ch->mRotationKeys[k].mValue.y,
                        (float)ch->mRotationKeys[k].mValue.z,
                        (float)ch->mRotationKeys[k].mValue.w);
                    channel.rotations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumScalingKeys; k++) {
                    BoneKeyFrame kf; kf.time = ch->mScalingKeys[k].mTime;
                    kf.s = XMFLOAT3((float)ch->mScalingKeys[k].mValue.x,
                        (float)ch->mScalingKeys[k].mValue.y,
                        (float)ch->mScalingKeys[k].mValue.z);
                    channel.scalings.push_back(kf);
                }
                clip.channels.push_back(std::move(channel));
            }
            m_clips.push_back(std::move(clip));
        }
    }
    for (uint32_t i = 0; i < MODEL_MAX_BONES; i++) m_finalBoneMatrices[i] = XMMatrixIdentity();

    if (m_bones.size() > MODEL_MAX_BONES) {
        OutputDebugStringA("[ModelComponent] Warning: bone count exceeds MODEL_MAX_BONES\n");
    }
    return true;
}

// ---------- ノード / メッシュ ----------
void ModelComponent::ProcessNode(aiNode* node, const aiScene* scene, int parentIndex) {
    int thisIndex = -1;
    auto it = m_boneNameToIndex.find(node->mName.C_Str());
    if (it != m_boneNameToIndex.end()) thisIndex = it->second;
    else {
        Bone b;
        b.name = node->mName.C_Str();
        b.parent = parentIndex;
        b.offset = XMMatrixIdentity();
        b.local = AiToXM(node->mTransformation);
        b.global = XMMatrixIdentity();
        thisIndex = (int)m_bones.size();
        m_bones.push_back(b);
        m_boneNameToIndex[b.name] = thisIndex;
    }
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }
    for (uint32_t c = 0; c < node->mNumChildren; c++) {
        ProcessNode(node->mChildren[c], scene, thisIndex);
    }
}

void ModelComponent::ProcessMesh(const aiMesh* mesh, const aiScene* /*scene*/) {
    SubMesh sm;
    sm.materialIndex = mesh->mMaterialIndex;
    sm.vertexOffset = (uint32_t)m_vertices.size();
    sm.indexOffset = (uint32_t)m_indices.size();
    sm.skinned = (mesh->mNumBones > 0);

    // 頂点
    for (uint32_t v = 0; v < mesh->mNumVertices; v++) {
        ModelVertex vert{};
        vert.position = XMFLOAT3(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z);
        vert.normal = mesh->HasNormals()
            ? XMFLOAT3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z)
            : XMFLOAT3(0, 1, 0);
        if (mesh->mTangents) {
            vert.tangent = XMFLOAT4(mesh->mTangents[v].x, mesh->mTangents[v].y, mesh->mTangents[v].z, 1.0f);
        }
        else {
            vert.tangent = XMFLOAT4(1, 0, 0, 1);
        }
        if (mesh->mTextureCoords[0]) {
            vert.uv = XMFLOAT2(mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y);
        }
        else {
            vert.uv = XMFLOAT2(0, 0);
        }
        if (mesh->mNumBones == 0) {
            vert.boneWeights[0] = 1.0f;
            vert.boneIndices[0] = 0;
        }
        m_vertices.push_back(vert);
    }

    // ボーン
    if (mesh->mNumBones > 0) {
        for (uint32_t b = 0; b < mesh->mNumBones; b++) {
            aiBone* aibone = mesh->mBones[b];
            int boneIndex;
            auto it = m_boneNameToIndex.find(aibone->mName.C_Str());
            if (it == m_boneNameToIndex.end()) {
                Bone bone;
                bone.name = aibone->mName.C_Str();
                bone.parent = -1;
                bone.offset = AiToXM(aibone->mOffsetMatrix);
                bone.local = XMMatrixIdentity();
                bone.global = XMMatrixIdentity();
                boneIndex = (int)m_bones.size();
                m_bones.push_back(bone);
                m_boneNameToIndex[bone.name] = boneIndex;
            }
            else {
                boneIndex = it->second;
                m_bones[boneIndex].offset = AiToXM(aibone->mOffsetMatrix);
            }
            for (uint32_t w = 0; w < aibone->mNumWeights; w++) {
                auto& vw = aibone->mWeights[w];
                uint32_t vId = sm.vertexOffset + vw.mVertexId;
                float weight = vw.mWeight;
                for (int s = 0; s < MODEL_MAX_INFLUENCES; s++) {
                    if (m_vertices[vId].boneWeights[s] == 0.0f) {
                        m_vertices[vId].boneIndices[s] = boneIndex;
                        m_vertices[vId].boneWeights[s] = weight;
                        break;
                    }
                }
            }
        }
        // 正規化
        for (uint32_t v = 0; v < mesh->mNumVertices; v++) {
            float sum = 0.f;
            for (int s = 0; s < MODEL_MAX_INFLUENCES; s++)
                sum += m_vertices[sm.vertexOffset + v].boneWeights[s];
            if (sum > 0.f) {
                float inv = 1.f / sum;
                for (int s = 0; s < MODEL_MAX_INFLUENCES; s++) {
                    m_vertices[sm.vertexOffset + v].boneWeights[s] *= inv;
                }
            }
            else {
                m_vertices[sm.vertexOffset + v].boneWeights[0] = 1.0f;
            }
        }
    }

    // インデックス
    for (uint32_t f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;
        m_indices.push_back(sm.vertexOffset + face.mIndices[0]);
        m_indices.push_back(sm.vertexOffset + face.mIndices[1]);
        m_indices.push_back(sm.vertexOffset + face.mIndices[2]);
        sm.indexCount += 3;
    }
    m_subMeshes.push_back(sm);
}

// ---------- GPU セットアップ ----------
void ModelComponent::BuildBuffers() {
    ID3D11Device* device = DirectX11::GetInstance()->GetDevice();
    if (!device) return;

    if (!m_vertices.empty()) {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = (UINT)(sizeof(ModelVertex) * m_vertices.size());
        D3D11_SUBRESOURCE_DATA init{ m_vertices.data(),0,0 };
        device->CreateBuffer(&bd, &init, m_vertexBuffer.GetAddressOf());

        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.ByteWidth = (UINT)(sizeof(uint32_t) * m_indices.size());
        init.pSysMem = m_indices.data();
        device->CreateBuffer(&bd, &init, m_indexBuffer.GetAddressOf());
    }

    if (!m_unifiedVS.empty()) {
        EnsureInputLayout(m_unifiedVS);
    }
}

void ModelComponent::EnsureInputLayout(const std::string& vsName) {
    if (vsName.empty()) return;
    if (m_inputLayout && vsName == m_unifiedVS) return;

    const void* bytecode = nullptr; size_t size = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(vsName, &bytecode, &size)) return;
    ID3D11Device* device = DirectX11::GetInstance()->GetDevice();
    if (!device) return;

    D3D11_INPUT_ELEMENT_DESC descs[] = {
        {"POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,     0, (UINT)offsetof(ModelVertex,position),     D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,   DXGI_FORMAT_R32G32B32_FLOAT,     0, (UINT)offsetof(ModelVertex,normal),       D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0,  DXGI_FORMAT_R32G32B32A32_FLOAT,  0, (UINT)offsetof(ModelVertex,tangent),      D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,        0, (UINT)offsetof(ModelVertex,uv),           D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEINDICES",0, DXGI_FORMAT_R32G32B32A32_UINT,0, (UINT)offsetof(ModelVertex,boneIndices),  D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEWEIGHTS",0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(ModelVertex,boneWeights),  D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    m_inputLayout.Reset();
    device->CreateInputLayout(descs, _countof(descs), bytecode, size, m_inputLayout.GetAddressOf());
}

// ---------- シェーダ / テクスチャ ----------
void ModelComponent::SetUnifiedShader(const std::string& vs, const std::string& ps) {
    m_unifiedVS = vs;
    m_unifiedPS = ps;
    for (auto& m : m_materials) {
        if (!m.overrideUnified) {
            m.vertexShader = vs;
            m.pixelShader = ps;
        }
    }
    EnsureInputLayout(vs);
}

void ModelComponent::SetMaterialShader(uint32_t materialIndex, const std::string& vs, const std::string& ps, bool setOverride) {
    if (materialIndex >= m_materials.size()) return;
    m_materials[materialIndex].vertexShader = vs;
    m_materials[materialIndex].pixelShader = ps;
    if (setOverride) m_materials[materialIndex].overrideUnified = true;
    EnsureInputLayout(vs);
}

bool ModelComponent::LoadTextureFromAsset(const std::string& assetName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV) {
    auto itC = m_textureCache.find(assetName);
    if (itC != m_textureCache.end()) {
        outSRV = itC->second;
        return true;
    }

    std::vector<uint8_t> data;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, data)) return false;

    ScratchImage img;
    auto pos = assetName.find_last_of('.');
    std::string ext = (pos == std::string::npos) ? "" : assetName.substr(pos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    HRESULT hr = E_FAIL;
    if (ext == "dds") {
        hr = LoadFromDDSMemory(data.data(), data.size(), DDS_FLAGS_NONE, nullptr, img);
    }
    else {
        hr = LoadFromWICMemory(data.data(), data.size(), WIC_FLAGS_FORCE_SRGB, nullptr, img);
    }
    if (FAILED(hr)) return false;

    ScratchImage mipChain;
    const TexMetadata& meta0 = img.GetMetadata();
    if (meta0.mipLevels <= 1) {
        if (SUCCEEDED(GenerateMipMaps(img.GetImages(), img.GetImageCount(), meta0,
            TEX_FILTER_DEFAULT, 0, mipChain))) {
            img = std::move(mipChain);
        }
    }

    ID3D11Device* device = DirectX11::GetInstance()->GetDevice();
    if (!device) return false;

    hr = CreateShaderResourceView(device, img.GetImages(), img.GetImageCount(), img.GetMetadata(),
        outSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    m_textureCache[assetName] = outSRV;
    return true;
}

bool ModelComponent::SetMaterialTexture(uint32_t materialIndex, uint32_t slot, const std::string& assetName) {
    if (materialIndex >= m_materials.size()) return false;
    if (slot >= m_materials[materialIndex].textures.size())
        m_materials[materialIndex].textures.resize(slot + 1);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (!LoadTextureFromAsset(assetName, srv)) {
        std::string dbg = "[ModelComponent] SetMaterialTexture failed: " + assetName + "\n";
        OutputDebugStringA(dbg.c_str());
        return false;
    }
    m_materials[materialIndex].textures[slot].srv = srv;
    m_materials[materialIndex].textures[slot].assetName = assetName;
    return true;
}

// ---------- アニメーション ----------
void ModelComponent::PlayAnimation(const std::string& clipName, bool loop, float speed) {
    for (int i = 0; i < (int)m_clips.size(); i++) {
        if (m_clips[i].name == clipName) {
            m_currentClip = i;
            m_loop = loop;
            m_animSpeed = speed;
            m_animTime = 0.0;
            return;
        }
    }
}

void ModelComponent::StopAnimation() { m_currentClip = -1; }
void ModelComponent::SetAnimationTime(double time) { m_animTime = time; }

int ModelComponent::FindBoneIndex(const std::string& name) const {
    auto it = m_boneNameToIndex.find(name);
    return (it == m_boneNameToIndex.end()) ? -1 : it->second;
}

void ModelComponent::InterpolateChannel(const BoneAnimChannel& ch, double time,
    XMFLOAT3& outT, XMFLOAT4& outR, XMFLOAT3& outS)
{
    auto interpVec3 = [&](const std::vector<BoneKeyFrame>& keys, XMFLOAT3& out) {
        if (keys.empty()) return;
        if (keys.size() == 1) { out = keys[0].t; return; }
        size_t k = 0;
        while (k + 1 < keys.size() && keys[k + 1].time < time) k++;
        size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
        double t0 = keys[k].time; double t1 = keys[k2].time;
        double alpha = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0;
        out.x = (float)(keys[k].t.x + (keys[k2].t.x - keys[k].t.x) * alpha);
        out.y = (float)(keys[k].t.y + (keys[k2].t.y - keys[k].t.y) * alpha);
        out.z = (float)(keys[k].t.z + (keys[k2].t.z - keys[k].t.z) * alpha);
        };
    auto interpQuat = [&](const std::vector<BoneKeyFrame>& keys, XMFLOAT4& out) {
        if (keys.empty()) return;
        if (keys.size() == 1) { out = keys[0].r; return; }
        size_t k = 0;
        while (k + 1 < keys.size() && keys[k + 1].time < time) k++;
        size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
        double t0 = keys[k].time; double t1 = keys[k2].time;
        double alpha = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0;
        XMVECTOR q0 = XMLoadFloat4(&keys[k].r);
        XMVECTOR q1 = XMLoadFloat4(&keys[k2].r);
        XMVECTOR q = XMQuaternionSlerp(q0, q1, (float)alpha);
        XMStoreFloat4(&out, q);
        };
    auto interpScale = interpVec3;

    interpVec3(ch.translations, outT);
    interpQuat(ch.rotations, outR);
    interpScale(ch.scalings, outS);
}

void ModelComponent::ApplyAnimationToBones(double timeInTicks) {
    if (m_currentClip < 0) return;
    auto& clip = m_clips[m_currentClip];
    for (auto& channel : clip.channels) {
        int bIndex = FindBoneIndex(channel.boneName);
        if (bIndex < 0) continue;
        XMFLOAT3 T{ 0,0,0 }, S{ 1,1,1 }; XMFLOAT4 R{ 0,0,0,1 };
        InterpolateChannel(channel, timeInTicks, T, R, S);
        XMVECTOR t = XMLoadFloat3(&T);
        XMVECTOR r = XMLoadFloat4(&R);
        XMVECTOR s = XMLoadFloat3(&S);
        m_bones[bIndex].local = XMMatrixAffineTransformation(s, XMVectorZero(), r, t);
    }
}

void ModelComponent::UpdateBoneMatrices() {
    if (m_bones.empty()) return;
    for (size_t i = 0; i < m_bones.size(); i++) {
        if (m_bones[i].parent < 0) m_bones[i].global = m_bones[i].local;
        else m_bones[i].global = m_bones[m_bones[i].parent].global * m_bones[i].local;
    }
    for (size_t i = 0; i < m_bones.size() && i < MODEL_MAX_BONES; i++) {
        m_finalBoneMatrices[i] = m_bones[i].offset * m_bones[i].global;
    }
}

void ModelComponent::UpdateAnimation(double deltaTime) {
    if (m_currentClip < 0) return;
    auto& clip = m_clips[m_currentClip];
    double ticks = deltaTime * clip.ticksPerSecond * m_animSpeed;
    m_animTime += ticks;
    if (m_loop) {
        while (m_animTime > clip.duration) m_animTime -= clip.duration;
    }
    else {
        if (m_animTime > clip.duration) m_animTime = clip.duration;
    }
    ApplyAnimationToBones(m_animTime);
    UpdateBoneMatrices();
}

// ---------- IK ----------
void ModelComponent::SolveIK_CCD(const std::vector<int>& chain, const XMFLOAT3& targetWorld, int iterations, float thresholdDeg) {
    if (chain.size() < 2) return;
    XMVECTOR target = XMLoadFloat3(&targetWorld);
    for (int it = 0; it < iterations; ++it) {
        int endBone = chain.back();
        XMVECTOR endPos = m_bones[endBone].global.r[3];
        float dist = XMVectorGetX(XMVector3Length(target - endPos));
        if (dist < 0.0001f) break;

        for (int c = (int)chain.size() - 2; c >= 0; --c) {
            int boneIndex = chain[c];
            XMVECTOR pivot = m_bones[boneIndex].global.r[3];
            endPos = m_bones[endBone].global.r[3];
            XMVECTOR vCur = XMVector3Normalize(endPos - pivot);
            XMVECTOR vTar = XMVector3Normalize(target - pivot);
            float dot = XMVectorGetX(XMVector3Dot(vCur, vTar));
            dot = std::clamp(dot, -1.0f, 1.0f);
            float angleRad = acosf(dot);
            float angleDeg = angleRad * 180.0f / 3.14159265f;
            if (angleDeg < thresholdDeg) continue;
            XMVECTOR axis = XMVector3Normalize(XMVector3Cross(vCur, vTar));
            if (XMVectorGetX(XMVector3Length(axis)) < 0.0001f) continue;
            XMMATRIX rot = XMMatrixRotationAxis(axis, angleRad);
            int parent = m_bones[boneIndex].parent;
            XMMATRIX parentInv = XMMatrixIdentity();
            if (parent >= 0) parentInv = XMMatrixInverse(nullptr, m_bones[parent].global);
            m_bones[boneIndex].local = parentInv * rot * m_bones[boneIndex].global;
            UpdateBoneMatrices();
        }
    }
}

// ---------- 更新 ----------
void ModelComponent::EditUpdate() {
    static auto prev = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    double dt = std::chrono::duration<double>(now - prev).count();
    prev = now;
    UpdateAnimation(dt);
}
void ModelComponent::InGameUpdate() {
    static auto prev = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    double dt = std::chrono::duration<double>(now - prev).count();
    prev = now;
    UpdateAnimation(dt);
}

// ---------- 描画 ----------
void ModelComponent::Draw() {
    if (!m_modelLoaded) return;
    ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
    if (!ctx) return;

    DirectX11::GetInstance()->SetBlendMode(BLEND_NONE);

    UINT stride = sizeof(ModelVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = m_vertexBuffer.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
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

    if (m_enableSkinning && !m_bones.empty()) {
        std::string vsName = m_unifiedVS;
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::VS, vsName, "SkinCB", "gBones",
            m_finalBoneMatrices, (UINT)(sizeof(XMMATRIX) * MODEL_MAX_BONES)
        );
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, vsName);
    }

    for (auto& sm : m_subMeshes) {
        if (sm.materialIndex >= m_materials.size()) continue;
        auto& mat = m_materials[sm.materialIndex];

        auto* vs = ShaderManager::GetInstance()->GetVertexShader(mat.vertexShader);
        auto* ps = ShaderManager::GetInstance()->GetPixelShader(mat.pixelShader);
        if (!vs || !ps) continue;

        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);

        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::VS, mat.vertexShader, "CameraCB", "world", &wT, sizeof(wT));
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::VS, mat.vertexShader, "CameraCB", "view", &viewT, sizeof(viewT));
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::VS, mat.vertexShader, "CameraCB", "proj", &projT, sizeof(projT));
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, mat.vertexShader);

        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::PS, mat.pixelShader, "MaterialCB", "BaseColor",
            &mat.baseColor, sizeof(XMFLOAT4));

        struct MR { float metallic; float roughness; XMFLOAT2 pad; } mr{ mat.metallic, mat.roughness,{0,0} };
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::PS, mat.pixelShader, "MaterialCB", "MetallicRoughness",
            &mr, sizeof(MR));

        UINT useBaseTex = 0;
        if (!mat.textures.empty() && mat.textures.size() > 0 && mat.textures[0].srv)
            useBaseTex = 1;
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::PS, mat.pixelShader, "MaterialCB", "UseBaseTex",
            &useBaseTex, sizeof(UINT));

        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::PS, mat.pixelShader);

        if (!mat.textures.empty()) {
            std::vector<ID3D11ShaderResourceView*> srvs;
            for (auto& ts : mat.textures) srvs.push_back(ts.srv.Get());
            ctx->PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
        }

        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }
}
// ---------- Inspector ----------
void ModelComponent::DrawInspector() {
    auto gui = EditrGUI::GetInstance();
    std::string header = gui->ShiftJISToUTF8("ModelComponent");
    std::string ptrId = std::to_string((uintptr_t)this);
    header += "###MC" + ptrId;

    if (!ImGui::CollapsingHeader(header.c_str())) return;

    // Transform
    if (_Parent) {
        Transform tr = _Parent->GetTransform();
        if (ImGui::BeginTable(("TransformTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("位置").c_str());
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3((std::string("##Pos") + ptrId).c_str(), &tr.position.x, 0.01f);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("回転").c_str());
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3((std::string("##Rot") + ptrId).c_str(), &tr.rotation.x, 0.01f);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("拡縮").c_str());
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3((std::string("##Scl") + ptrId).c_str(), &tr.scale.x, 0.01f, 0.001f, 1000.0f);
            ImGui::EndTable();
        }
        _Parent->SetTransform(tr);
    }

    // 2) モデルアセット選択
    if (ImGui::BeginTable(("ModelAssetTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("フィルタ").c_str());
        ImGui::TableSetColumnIndex(1); ImGui::InputText((std::string("##Filter") + ptrId).c_str(), m_modelSearch, sizeof(m_modelSearch));

        // アセット一覧
        std::string filter = m_modelSearch;
        std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);
        std::vector<std::string> models;
        for (auto& a : AssetsManager::GetInstance()->ListAssets()) {
            if (!IsModelAssetName(a)) continue;
            if (!filter.empty()) {
                std::string low = a; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                if (low.find(filter) == std::string::npos) continue;
            }
            models.push_back(a);
        }

        static int selectedIdx = -1;
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("アセット").c_str());
        ImGui::TableSetColumnIndex(1);
        {
            ImGui::BeginChild((std::string("##ModelList") + ptrId).c_str(), ImVec2(0, 100), true);
            for (int i = 0; i < (int)models.size(); i++) {
                bool sel = (i == selectedIdx);
                if (ImGui::Selectable((models[i] + "##Sel" + ptrId + std::to_string(i)).c_str(), sel)) {
                    selectedIdx = i;
                    strncpy_s(m_modelAssetInput, models[i].c_str(), _TRUNCATE);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (selectedIdx >= 0) LoadModelFromAsset(models[selectedIdx], true);
                }
            }
            ImGui::EndChild();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("手入力").c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::InputText((std::string("##ManualAsset") + ptrId).c_str(), m_modelAssetInput, sizeof(m_modelAssetInput));

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("操作").c_str());
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button((gui->ShiftJISToUTF8("読込") + "##LoadBtn" + ptrId).c_str())) {
            if (m_modelAssetInput[0]) LoadModelFromAsset(m_modelAssetInput, true);
        }
        ImGui::SameLine();
        if (ImGui::Button((gui->ShiftJISToUTF8("再読込") + "##ReloadBtn" + ptrId).c_str())) {
            if (!m_currentModelAsset.empty()) LoadModelFromAsset(m_currentModelAsset, true);
        }
        ImGui::SameLine();
        if (ImGui::Button((gui->ShiftJISToUTF8("解除") + "##ClearBtn" + ptrId).c_str())) {
            ClearModel();
        }

        if (m_modelLoaded) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("現在").c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_currentModelAsset.c_str());

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Stats");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("V:%zu I:%zu Sub:%zu", m_vertices.size(), m_indices.size(), m_subMeshes.size());
        }
        else {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Status");
            ImGui::TableSetColumnIndex(1); ImGui::Text(gui->ShiftJISToUTF8("未ロード").c_str());
        }
        ImGui::EndTable();
    }

    // 3) 統一シェーダ設定
    if (ImGui::BeginTable(("UnifiedShaderTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
        auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
        static int vsSel = 0, psSel = 0;
        for (int i = 0; i < (int)vsList.size(); ++i) if (vsList[i] == m_unifiedVS) vsSel = i;
        for (int i = 0; i < (int)psList.size(); ++i) if (psList[i] == m_unifiedPS) psSel = i;

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("VS");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginCombo((std::string("##VSCombo") + ptrId).c_str(), vsList.empty() ? "-" : vsList[vsSel].c_str())) {
            for (int i = 0; i < (int)vsList.size(); ++i) {
                bool sel = (i == vsSel);
                if (ImGui::Selectable((vsList[i] + "##VSItem" + ptrId + std::to_string(i)).c_str(), sel)) vsSel = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("PS");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginCombo((std::string("##PSCombo") + ptrId).c_str(), psList.empty() ? "-" : psList[psSel].c_str())) {
            for (int i = 0; i < (int)psList.size(); ++i) {
                bool sel = (i == psSel);
                if (ImGui::Selectable((psList[i] + "##PSItem" + ptrId + std::to_string(i)).c_str(), sel)) psSel = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("適用").c_str());
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button((gui->ShiftJISToUTF8("統一シェーダ反映") + "##ApplyUni" + ptrId).c_str())) {
            if (!vsList.empty() && !psList.empty())
                SetUnifiedShader(vsList[vsSel], psList[psSel]);
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Skinning");
        ImGui::TableSetColumnIndex(1); ImGui::Checkbox((std::string("##SkinEnable") + ptrId).c_str(), &m_enableSkinning);
        ImGui::EndTable();
    }

    // 4) マテリアル（選択式）
    if (m_materials.size() > 0) {
        if (ImGui::BeginTable(("MaterialTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Index");
            ImGui::TableSetColumnIndex(1); ImGui::DragInt((std::string("##MatIndex") + ptrId).c_str(), &m_selectedMaterial, 1, 0, (int)m_materials.size() - 1);

            m_selectedMaterial = std::clamp(m_selectedMaterial, 0, (int)m_materials.size() - 1);
            auto& mat = m_materials[m_selectedMaterial];

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Override");
            ImGui::TableSetColumnIndex(1); ImGui::Checkbox((std::string("##MatOv") + ptrId).c_str(), &mat.overrideUnified);

            // シェーダ
            auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
            auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
            static int vsLocal = 0, psLocal = 0;
            for (int i = 0; i < (int)vsList.size(); ++i) if (vsList[i] == mat.vertexShader) vsLocal = i;
            for (int i = 0; i < (int)psList.size(); ++i) if (psList[i] == mat.pixelShader) psLocal = i;

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("VS");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo((std::string("##MatVSCombo") + ptrId).c_str(), vsList.empty() ? "-" : vsList[vsLocal].c_str())) {
                for (int i = 0; i < (int)vsList.size(); ++i) {
                    bool sel = (i == vsLocal);
                    if (ImGui::Selectable((vsList[i] + "##MatVSItem" + ptrId + std::to_string(i)).c_str(), sel)) vsLocal = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("PS");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo((std::string("##MatPSCombo") + ptrId).c_str(), psList.empty() ? "-" : psList[psLocal].c_str())) {
                for (int i = 0; i < (int)psList.size(); ++i) {
                    bool sel = (i == psLocal);
                    if (ImGui::Selectable((psList[i] + "##MatPSItem" + ptrId + std::to_string(i)).c_str(), sel)) psLocal = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("反映").c_str());
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button((gui->ShiftJISToUTF8("変更適用") + "##MatApply" + ptrId).c_str())) {
                if (!vsList.empty() && !psList.empty())
                    SetMaterialShader((uint32_t)m_selectedMaterial, vsList[vsLocal], psList[psLocal], true);
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("BaseColor");
            ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4((std::string("##MatCol") + ptrId).c_str(), &mat.baseColor.x);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Metallic");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat((std::string("##MatMetal") + ptrId).c_str(), &mat.metallic, 0.01f, 0.0f, 1.0f);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Roughness");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat((std::string("##MatRough") + ptrId).c_str(), &mat.roughness, 0.01f, 0.0f, 1.0f);

            // テクスチャ
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("TexSlot");
            ImGui::TableSetColumnIndex(1);
            ImGui::InputText((std::string("##NewTexName") + ptrId).c_str(), m_newTextureName, sizeof(m_newTextureName));
            static int texSlot = 0;
            ImGui::DragInt((std::string("##TexSlot") + ptrId).c_str(), &texSlot, 1, 0, 16);
            if (ImGui::Button((gui->ShiftJISToUTF8("読み込み") + "##LoadTex" + ptrId).c_str())) {
                if (m_newTextureName[0])
                    SetMaterialTexture((uint32_t)m_selectedMaterial, (uint32_t)texSlot, m_newTextureName);
            }

            // 登録済みテクスチャ表示
            if (!mat.textures.empty()) {
                std::string joined;
                for (size_t i = 0; i < mat.textures.size(); ++i) {
                    if (!mat.textures[i].assetName.empty()) {
                        joined += "[" + std::to_string(i) + "]" + mat.textures[i].assetName + " ";
                    }
                }
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                ImGui::Text("SRVs");
                ImGui::TableSetColumnIndex(1);
                ImGui::TextWrapped("%s", joined.c_str());
            }

            ImGui::EndTable();
        }
    }

    // 5) アニメーション
    if (ImGui::BeginTable(("AnimationTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        if (m_clips.empty()) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Anim");
            ImGui::TableSetColumnIndex(1); ImGui::Text(gui->ShiftJISToUTF8("なし").c_str());
        }
        else {
            // クリップ選択 Combo
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Clip");
            ImGui::TableSetColumnIndex(1);
            if (m_selectedAnimIndex < 0 || m_selectedAnimIndex >= (int)m_clips.size()) m_selectedAnimIndex = 0;
            std::string curName = m_clips[m_selectedAnimIndex].name;
            if (ImGui::BeginCombo((std::string("##AnimCombo") + ptrId).c_str(), curName.c_str())) {
                for (int i = 0; i < (int)m_clips.size(); ++i) {
                    bool sel = (i == m_selectedAnimIndex);
                    if (ImGui::Selectable((m_clips[i].name + "##AnimItem" + ptrId + std::to_string(i)).c_str(), sel)) {
                        m_selectedAnimIndex = i;
                        PlayAnimation(m_clips[i].name, m_loop, (float)m_animSpeed);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // ループ / スピード
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Loop");
            ImGui::TableSetColumnIndex(1); ImGui::Checkbox((std::string("##LoopChk") + ptrId).c_str(), &m_loop);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Speed");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat((std::string("##AnimSpeed") + ptrId).c_str(), (float*)&m_animSpeed, 0.01f, 0.0f, 10.0f);

            if (m_currentClip >= 0) {
                auto& clip = m_clips[m_currentClip];
                float curT = (float)m_animTime;
                float maxT = (float)clip.duration;
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Time");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderFloat((std::string("##AnimTime") + ptrId).c_str(), &curT, 0.0f, maxT)) {
                    m_animTime = curT;
                    ApplyAnimationToBones(m_animTime);
                    UpdateBoneMatrices();
                }

                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text(gui->ShiftJISToUTF8("操作").c_str());
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button((gui->ShiftJISToUTF8("先頭") + "##AnimRestart" + ptrId).c_str()))
                    m_animTime = 0.0;
                ImGui::SameLine();
                if (ImGui::Button((gui->ShiftJISToUTF8("停止") + "##AnimStop" + ptrId).c_str()))
                    StopAnimation();
            }
        }
        ImGui::EndTable();
    }

    // 6) IK
    if (ImGui::BeginTable(("IKTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Chain");
        ImGui::TableSetColumnIndex(1); ImGui::InputText((std::string("##IKChain") + ptrId).c_str(), m_ikChainInput, sizeof(m_ikChainInput));

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Target");
        ImGui::TableSetColumnIndex(1); ImGui::DragFloat3((std::string("##IKTarget") + ptrId).c_str(), &m_ikTarget.x, 0.01f);

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Iter");
        ImGui::TableSetColumnIndex(1); ImGui::DragInt((std::string("##IKIter") + ptrId).c_str(), &m_ikIterations, 1, 1, 200);

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Thresh");
        ImGui::TableSetColumnIndex(1); ImGui::DragFloat((std::string("##IKTh") + ptrId).c_str(), &m_ikThresholdDeg, 0.01f, 0.0f, 20.0f);

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("CCD");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button((gui->ShiftJISToUTF8("IK解決") + "##IKSolve" + ptrId).c_str())) {
            std::vector<int> chain;
            std::stringstream ss(m_ikChainInput);
            std::string seg;
            while (std::getline(ss, seg, ',')) {
                if (!seg.empty()) chain.push_back(std::atoi(seg.c_str()));
            }
            if (chain.size() >= 2)
                SolveIK_CCD(chain, m_ikTarget, m_ikIterations, m_ikThresholdDeg);
        }
        ImGui::EndTable();
    }

    // 7) 情報
    if (ImGui::BeginTable(("InfoTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Loaded");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_currentModelAsset.empty() ? "(None)" : m_currentModelAsset.c_str());

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Bones");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%zu", m_bones.size());

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Clips");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%zu", m_clips.size());

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("CurClip");
        ImGui::TableSetColumnIndex(1);
        if (m_currentClip >= 0) ImGui::Text("%s", m_clips[m_currentClip].name.c_str()); else ImGui::Text("-");
        ImGui::EndTable();
    }
}