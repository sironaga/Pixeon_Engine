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

// ---------------- コンストラクタ / デストラクタ ----------------
ModelComponent::ModelComponent() {
    _ComponentName = "ModelComponent";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
    m_modelAssetInput[0] = 0;
    m_modelSearch[0] = 0;
    m_newTextureName[0] = 0;
    m_ikChainInput[0] = 0;
}
ModelComponent::~ModelComponent() { ClearModel(); }

// ---------------- ライフサイクル ----------------
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

    std::vector<uint8_t> bin;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, bin)) {
        OutputDebugStringA(("[ModelComponent] LoadAsset failed: " + assetName + "\n").c_str());
        return false;
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(
        bin.data(), bin.size(),
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
        OutputDebugStringA("[ModelComponent] Assimp parse failed\n");
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

// ---------------- マテリアル色抽出 ----------------
void ModelComponent::ExtractMaterialColors(aiMaterial* aimat, ModelMaterial& outMat) {
    if (!aimat) return;
    aiColor4D c;
#ifdef AI_MATKEY_BASE_COLOR
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_BASE_COLOR, c)) {
        outMat.baseColor = { c.r,c.g,c.b,c.a };
    }
    else
#endif
        if (AI_SUCCESS == aimat->Get(AI_MATKEY_COLOR_DIFFUSE, c)) {
            outMat.baseColor = { c.r,c.g,c.b,c.a };
        }
#ifdef AI_MATKEY_METALLIC_FACTOR
    float m = 0.f;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_METALLIC_FACTOR, m)) outMat.metallic = m;
#endif
#ifdef AI_MATKEY_ROUGHNESS_FACTOR
    float r = 0.f;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_ROUGHNESS_FACTOR, r)) outMat.roughness = r;
#endif
}

// ---------------- テクスチャパス正規化 ----------------
std::string ModelComponent::NormalizeTexturePath(const std::string& raw, const std::string& modelDir) {
    if (raw.empty()) return raw;
    if (raw[0] == '*') return raw; // 埋め込みテクスチャ
    std::string p = raw;
    std::replace(p.begin(), p.end(), '\\', '/');
    if (p.size() > 2 && p[1] == ':') return p; // 絶対パス (Windows)
    if (!modelDir.empty()) {
        if (p.rfind(modelDir + "/", 0) == 0) return p;
        return modelDir + "/" + p;
    }
    return p;
}

// ---------------- Assimp シーン取り込み ----------------
bool ModelComponent::ImportAssimpScene(const std::string& assetName, const aiScene* scene) {
    std::string modelDir;
    if (auto pos = assetName.find_last_of("/\\"); pos != std::string::npos)
        modelDir = assetName.substr(0, pos);

    m_materials.resize(scene->mNumMaterials);

    for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
        aiMaterial* aimat = scene->mMaterials[i];
        ModelMaterial mat;
        ExtractMaterialColors(aimat, mat);

        // 先にベース代入
        m_materials[i] = mat;

        // テクスチャ (Diffuse / BaseColor 優先)
        aiString path;
#ifdef AI_MATKEY_BASE_COLOR_TEXTURE
        if (aimat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
            aimat->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS) {
            std::string rel = NormalizeTexturePath(path.C_Str(), modelDir);
            SetMaterialTexture(i, 0, rel);
        }
        else
#endif
            if (aimat->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
                std::string rel = NormalizeTexturePath(path.C_Str(), modelDir);
                SetMaterialTexture(i, 0, rel);
            }
    }

    ProcessNode(scene->mRootNode, scene, -1);

    // アニメーション
    if (scene->mNumAnimations > 0) {
        m_clips.reserve(scene->mNumAnimations);
        for (uint32_t i = 0; i < scene->mNumAnimations; ++i) {
            aiAnimation* a = scene->mAnimations[i];
            AnimationClip clip;
            clip.name = a->mName.length ? a->mName.C_Str() : ("Anim" + std::to_string(i));
            clip.duration = a->mDuration;
            clip.ticksPerSecond = (a->mTicksPerSecond != 0.0) ? a->mTicksPerSecond : 25.0;
            for (uint32_t c = 0; c < a->mNumChannels; ++c) {
                aiNodeAnim* ch = a->mChannels[c];
                BoneAnimChannel bc;
                bc.boneName = ch->mNodeName.C_Str();
                for (uint32_t k = 0; k < ch->mNumPositionKeys; ++k) {
                    BoneKeyFrame kf; kf.time = ch->mPositionKeys[k].mTime;
                    kf.t = { (float)ch->mPositionKeys[k].mValue.x,(float)ch->mPositionKeys[k].mValue.y,(float)ch->mPositionKeys[k].mValue.z };
                    bc.translations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumRotationKeys; ++k) {
                    BoneKeyFrame kf; kf.time = ch->mRotationKeys[k].mTime;
                    kf.r = { (float)ch->mRotationKeys[k].mValue.x,(float)ch->mRotationKeys[k].mValue.y,(float)ch->mRotationKeys[k].mValue.z,(float)ch->mRotationKeys[k].mValue.w };
                    bc.rotations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumScalingKeys; ++k) {
                    BoneKeyFrame kf; kf.time = ch->mScalingKeys[k].mTime;
                    kf.s = { (float)ch->mScalingKeys[k].mValue.x,(float)ch->mScalingKeys[k].mValue.y,(float)ch->mScalingKeys[k].mValue.z };
                    bc.scalings.push_back(kf);
                }
                clip.channels.push_back(std::move(bc));
            }
            m_clips.push_back(std::move(clip));
        }
    }

    for (uint32_t i = 0; i < MODEL_MAX_BONES; ++i)
        m_finalBoneMatrices[i] = XMMatrixIdentity();

    if (m_bones.size() > MODEL_MAX_BONES)
        OutputDebugStringA("[ModelComponent] Warning: bone count exceeds limit\n");

    return true;
}

// ---------------- ノード処理 ----------------
void ModelComponent::ProcessNode(aiNode* node, const aiScene* scene, int parentIndex) {
    int thisIndex = -1;
    if (auto it = m_boneNameToIndex.find(node->mName.C_Str()); it != m_boneNameToIndex.end())
        thisIndex = it->second;
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

    for (uint32_t i = 0; i < node->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }
    for (uint32_t c = 0; c < node->mNumChildren; ++c)
        ProcessNode(node->mChildren[c], scene, thisIndex);
}

// ---------------- メッシュ処理 ----------------
void ModelComponent::ProcessMesh(const aiMesh* mesh, const aiScene*) {
    SubMesh sm;
    sm.materialIndex = mesh->mMaterialIndex;
    sm.vertexOffset = (uint32_t)m_vertices.size();
    sm.indexOffset = (uint32_t)m_indices.size();
    sm.skinned = (mesh->mNumBones > 0);

    for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
        ModelVertex vert{};
        vert.position = { (float)mesh->mVertices[v].x,(float)mesh->mVertices[v].y,(float)mesh->mVertices[v].z };
        vert.normal = mesh->HasNormals()
            ? XMFLOAT3((float)mesh->mNormals[v].x, (float)mesh->mNormals[v].y, (float)mesh->mNormals[v].z)
            : XMFLOAT3(0, 1, 0);
        if (mesh->mTangents)
            vert.tangent = { (float)mesh->mTangents[v].x,(float)mesh->mTangents[v].y,(float)mesh->mTangents[v].z,1.0f };
        else
            vert.tangent = { 1,0,0,1 };
        if (mesh->mTextureCoords[0])
            vert.uv = { (float)mesh->mTextureCoords[0][v].x,(float)mesh->mTextureCoords[0][v].y };
        else
            vert.uv = { 0,0 };

        if (mesh->mNumBones == 0) {
            vert.boneWeights[0] = 1.0f;
            vert.boneIndices[0] = 0;
        }
        m_vertices.push_back(vert);
    }

    if (mesh->mNumBones > 0) {
        for (uint32_t b = 0; b < mesh->mNumBones; ++b) {
            aiBone* aib = mesh->mBones[b];
            int boneIndex;
            if (auto it = m_boneNameToIndex.find(aib->mName.C_Str()); it == m_boneNameToIndex.end()) {
                Bone nb;
                nb.name = aib->mName.C_Str();
                nb.parent = -1;
                nb.offset = AiToXM(aib->mOffsetMatrix);
                nb.local = XMMatrixIdentity();
                nb.global = XMMatrixIdentity();
                boneIndex = (int)m_bones.size();
                m_bones.push_back(nb);
                m_boneNameToIndex[nb.name] = boneIndex;
            }
            else {
                boneIndex = it->second;
                m_bones[boneIndex].offset = AiToXM(aib->mOffsetMatrix);
            }
            for (uint32_t w = 0; w < aib->mNumWeights; ++w) {
                auto& vw = aib->mWeights[w];
                uint32_t vid = sm.vertexOffset + vw.mVertexId;
                for (int s = 0; s < (int)MODEL_MAX_INFLUENCES; ++s) {
                    if (m_vertices[vid].boneWeights[s] == 0.0f) {
                        m_vertices[vid].boneIndices[s] = boneIndex;
                        m_vertices[vid].boneWeights[s] = vw.mWeight;
                        break;
                    }
                }
            }
        }
        // 正規化
        for (uint32_t v = 0; v < mesh->mNumVertices; ++v) {
            float sum = 0.f;
            for (int s = 0; s < (int)MODEL_MAX_INFLUENCES; ++s)
                sum += m_vertices[sm.vertexOffset + v].boneWeights[s];
            if (sum > 0.f) {
                float inv = 1.f / sum;
                for (int s = 0; s < (int)MODEL_MAX_INFLUENCES; ++s)
                    m_vertices[sm.vertexOffset + v].boneWeights[s] *= inv;
            }
            else {
                m_vertices[sm.vertexOffset + v].boneWeights[0] = 1.0f;
            }
        }
    }

    for (uint32_t f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;
        m_indices.push_back(sm.vertexOffset + face.mIndices[0]);
        m_indices.push_back(sm.vertexOffset + face.mIndices[1]);
        m_indices.push_back(sm.vertexOffset + face.mIndices[2]);
        sm.indexCount += 3;
    }
    m_subMeshes.push_back(sm);
}

// ---------------- GPU バッファ生成 ----------------
void ModelComponent::BuildBuffers() {
    ID3D11Device* dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return;
    if (!m_vertices.empty()) {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = (UINT)(sizeof(ModelVertex) * m_vertices.size());
        D3D11_SUBRESOURCE_DATA init{ m_vertices.data(),0,0 };
        dev->CreateBuffer(&bd, &init, m_vertexBuffer.GetAddressOf());

        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.ByteWidth = (UINT)(sizeof(uint32_t) * m_indices.size());
        init.pSysMem = m_indices.data();
        dev->CreateBuffer(&bd, &init, m_indexBuffer.GetAddressOf());
    }
    if (!m_unifiedVS.empty())
        EnsureInputLayout(m_unifiedVS);
}

void ModelComponent::EnsureInputLayout(const std::string& vsName) {
    if (vsName.empty()) return;
    if (m_inputLayout && vsName == m_unifiedVS) return;

    const void* bc = nullptr; size_t sz = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(vsName, &bc, &sz)) return;
    auto* dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return;

    D3D11_INPUT_ELEMENT_DESC descs[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,     0,(UINT)offsetof(ModelVertex,position),    D3D11_INPUT_PER_VERTEX_DATA,0},
        {"NORMAL",0,  DXGI_FORMAT_R32G32B32_FLOAT,     0,(UINT)offsetof(ModelVertex,normal),      D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TANGENT",0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0,(UINT)offsetof(ModelVertex,tangent),     D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,        0,(UINT)offsetof(ModelVertex,uv),          D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEINDICES",0,DXGI_FORMAT_R32G32B32A32_UINT,0,(UINT)offsetof(ModelVertex,boneIndices), D3D11_INPUT_PER_VERTEX_DATA,0},
        {"BONEWEIGHTS",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(ModelVertex,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0},
    };
    m_inputLayout.Reset();
    dev->CreateInputLayout(descs, _countof(descs), bc, sz, m_inputLayout.GetAddressOf());
}

// ---------------- シェーダ設定 ----------------
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
    auto& m = m_materials[materialIndex];
    m.vertexShader = vs;
    m.pixelShader = ps;
    if (setOverride) m.overrideUnified = true;
    EnsureInputLayout(vs);
}

// ---------------- テクスチャ読み込み ----------------
bool ModelComponent::LoadTextureFromAsset(const std::string& assetName, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV) {
    if (auto it = m_textureCache.find(assetName); it != m_textureCache.end()) {
        outSRV = it->second;
        return true;
    }
    std::vector<uint8_t> data;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, data)) {
        OutputDebugStringA(("[ModelComponent] LoadTextureFromAsset file not found: " + assetName + "\n").c_str());
        return false;
    }

    ScratchImage img;
    std::string ext;
    if (auto p = assetName.find_last_of('.'); p != std::string::npos) {
        ext = assetName.substr(p + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    }
    HRESULT hr;
    if (ext == "dds") hr = LoadFromDDSMemory(data.data(), data.size(), DDS_FLAGS_NONE, nullptr, img);
    else            hr = LoadFromWICMemory(data.data(), data.size(), WIC_FLAGS_FORCE_SRGB, nullptr, img);
    if (FAILED(hr)) {
        OutputDebugStringA(("[ModelComponent] WIC/DDS load failed: " + assetName + "\n").c_str());
        return false;
    }

    if (img.GetMetadata().mipLevels <= 1) {
        ScratchImage mip;
        if (SUCCEEDED(GenerateMipMaps(img.GetImages(), img.GetImageCount(), img.GetMetadata(), TEX_FILTER_DEFAULT, 0, mip)))
            img = std::move(mip);
    }

    auto* dev = DirectX11::GetInstance()->GetDevice();
    hr = CreateShaderResourceView(dev, img.GetImages(), img.GetImageCount(), img.GetMetadata(), outSRV.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA(("[ModelComponent] CreateSRV failed: " + assetName + "\n").c_str());
        return false;
    }
    m_textureCache[assetName] = outSRV;
    return true;
}

bool ModelComponent::SetMaterialTexture(uint32_t materialIndex, uint32_t slot, const std::string& assetName) {
    if (materialIndex >= m_materials.size()) return false;
    auto& mat = m_materials[materialIndex];
    if (slot >= mat.textures.size())
        mat.textures.resize(slot + 1);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    if (!LoadTextureFromAsset(assetName, srv)) {
        OutputDebugStringA(("[ModelComponent] SetMaterialTexture failed: " + assetName + "\n").c_str());
        return false;
    }
    mat.textures[slot].srv = srv;
    mat.textures[slot].assetName = assetName;
    return true;
}

// ---------------- アニメーション補助 ----------------
int ModelComponent::FindBoneIndex(const std::string& name) const {
    if (auto it = m_boneNameToIndex.find(name); it != m_boneNameToIndex.end())
        return it->second;
    return -1;
}

void ModelComponent::InterpolateChannel(const BoneAnimChannel& ch, double t,
    XMFLOAT3& outT, XMFLOAT4& outR, XMFLOAT3& outS)
{
    auto interpV = [&](const std::vector<BoneKeyFrame>& keys, XMFLOAT3& o) {
        if (keys.empty()) return;
        if (keys.size() == 1) { o = keys[0].t; return; }
        size_t k = 0; while (k + 1 < keys.size() && keys[k + 1].time < t) k++;
        size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
        double t0 = keys[k].time, t1 = keys[k2].time;
        double a = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
        o.x = (float)(keys[k].t.x + (keys[k2].t.x - keys[k].t.x) * a);
        o.y = (float)(keys[k].t.y + (keys[k2].t.y - keys[k].t.y) * a);
        o.z = (float)(keys[k].t.z + (keys[k2].t.z - keys[k].t.z) * a);
        };
    auto interpQ = [&](const std::vector<BoneKeyFrame>& keys, XMFLOAT4& o) {
        if (keys.empty()) return;
        if (keys.size() == 1) { o = keys[0].r; return; }
        size_t k = 0; while (k + 1 < keys.size() && keys[k + 1].time < t) k++;
        size_t k2 = (k + 1 < keys.size()) ? k + 1 : k;
        double t0 = keys[k].time, t1 = keys[k2].time;
        double a = (t1 > t0) ? (t - t0) / (t1 - t0) : 0.0;
        XMVECTOR q0 = XMLoadFloat4(&keys[k].r);
        XMVECTOR q1 = XMLoadFloat4(&keys[k2].r);
        XMVECTOR q = XMQuaternionSlerp(q0, q1, (float)a);
        XMStoreFloat4(&o, q);
        };
    interpV(ch.translations, outT);
    interpQ(ch.rotations, outR);
    interpV(ch.scalings, outS);
}

void ModelComponent::ApplyAnimationToBones(double timeInTicks) {
    if (m_currentClip < 0) return;
    auto& clip = m_clips[m_currentClip];
    for (auto& ch : clip.channels) {
        int bi = FindBoneIndex(ch.boneName);
        if (bi < 0) continue;
        XMFLOAT3 T{ 0,0,0 }, S{ 1,1,1 }; XMFLOAT4 R{ 0,0,0,1 };
        InterpolateChannel(ch, timeInTicks, T, R, S);
        XMVECTOR t = XMLoadFloat3(&T), s = XMLoadFloat3(&S), r = XMLoadFloat4(&R);
        m_bones[bi].local = XMMatrixAffineTransformation(s, XMVectorZero(), r, t);
    }
}

void ModelComponent::UpdateBoneMatrices() {
    if (m_bones.empty()) return;
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].parent < 0) m_bones[i].global = m_bones[i].local;
        else m_bones[i].global = m_bones[m_bones[i].parent].global * m_bones[i].local;
    }
    for (size_t i = 0; i < m_bones.size() && i < MODEL_MAX_BONES; ++i)
        m_finalBoneMatrices[i] = m_bones[i].offset * m_bones[i].global;
}

void ModelComponent::PlayAnimation(const std::string& clipName, bool loop, float speed) {
    for (int i = 0; i < (int)m_clips.size(); ++i) {
        if (m_clips[i].name == clipName) {
            m_currentClip = i; m_loop = loop; m_animSpeed = speed; m_animTime = 0.0; return;
        }
    }
}
void ModelComponent::StopAnimation() { m_currentClip = -1; }
void ModelComponent::SetAnimationTime(double t) { m_animTime = t; }

void ModelComponent::UpdateAnimation(double dt) {
    if (m_currentClip < 0) return;
    auto& clip = m_clips[m_currentClip];
    double ticks = dt * clip.ticksPerSecond * m_animSpeed;
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

// ---------------- IK ----------------
void ModelComponent::SolveIK_CCD(const std::vector<int>& chain, const XMFLOAT3& targetWorld, int iterations, float thresholdDeg) {
    if (chain.size() < 2) return;
    XMVECTOR target = XMLoadFloat3(&targetWorld);
    for (int it = 0; it < iterations; ++it) {
        int endB = chain.back();
        XMVECTOR endPos = m_bones[endB].global.r[3];
        if (XMVectorGetX(XMVector3Length(target - endPos)) < 0.0001f) break;

        for (int c = (int)chain.size() - 2; c >= 0; --c) {
            int bi = chain[c];
            XMVECTOR pivot = m_bones[bi].global.r[3];
            endPos = m_bones[endB].global.r[3];
            XMVECTOR vCur = XMVector3Normalize(endPos - pivot);
            XMVECTOR vTar = XMVector3Normalize(target - pivot);
            float dot = std::clamp(XMVectorGetX(XMVector3Dot(vCur, vTar)), -1.0f, 1.0f);
            float ang = acosf(dot);
            if (ang * 180.0f / 3.14159265f < thresholdDeg) continue;
            XMVECTOR axis = XMVector3Normalize(XMVector3Cross(vCur, vTar));
            if (XMVectorGetX(XMVector3Length(axis)) < 0.0001f) continue;
            XMMATRIX rot = XMMatrixRotationAxis(axis, ang);
            int parent = m_bones[bi].parent;
            XMMATRIX pinv = XMMatrixIdentity();
            if (parent >= 0) pinv = XMMatrixInverse(nullptr, m_bones[parent].global);
            m_bones[bi].local = pinv * rot * m_bones[bi].global;
            UpdateBoneMatrices();
        }
    }
}

// ---------------- Update ----------------
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

// ---------------- 描画 ----------------
void ModelComponent::Draw() {
    if (!m_modelLoaded) return;
    auto* ctx = DirectX11::GetInstance()->GetContext();
    if (!ctx) return;

    DirectX11::GetInstance()->SetBlendMode(BLEND_NONE);

    UINT stride = sizeof(ModelVertex), offset = 0;
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
        ShaderManager::GetInstance()->SetCBufferVariable(
            ShaderStage::VS, m_unifiedVS, "SkinCB", "gBones",
            m_finalBoneMatrices, (UINT)(sizeof(XMMATRIX) * MODEL_MAX_BONES)
        );
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, m_unifiedVS);
    }

    for (auto& sm : m_subMeshes) {
        if (sm.materialIndex >= m_materials.size()) continue;
        auto& mat = m_materials[sm.materialIndex];

        auto* vs = ShaderManager::GetInstance()->GetVertexShader(mat.vertexShader);
        auto* ps = ShaderManager::GetInstance()->GetPixelShader(mat.pixelShader);
        if (!vs || !ps) continue;

        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);

        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vertexShader, "CameraCB", "world", &wT, sizeof(wT));
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vertexShader, "CameraCB", "view", &viewT, sizeof(viewT));
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::VS, mat.vertexShader, "CameraCB", "proj", &projT, sizeof(projT));
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::VS, mat.vertexShader);

        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.pixelShader, "MaterialCB", "BaseColor", &mat.baseColor, sizeof(mat.baseColor));
        struct MR { float metallic; float roughness; XMFLOAT2 pad; } mr{ mat.metallic, mat.roughness,{0,0} };
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.pixelShader, "MaterialCB", "MetallicRoughness", &mr, sizeof(MR));

        UINT useBase = (!mat.textures.empty() && mat.textures[0].srv) ? 1u : 0u;
        ShaderManager::GetInstance()->SetCBufferVariable(ShaderStage::PS, mat.pixelShader, "MaterialCB", "UseBaseTex", &useBase, sizeof(UINT));
        ShaderManager::GetInstance()->CommitAndBind(ShaderStage::PS, mat.pixelShader);

        if (!mat.textures.empty()) {
            std::vector<ID3D11ShaderResourceView*> srvs;
            srvs.reserve(mat.textures.size());
            for (auto& ts : mat.textures)
                srvs.push_back(ts.srv.Get());
            ctx->PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
        }

        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }
}

// ---------------- Inspector (シェーダ変更ロジック修正) ----------------
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
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Pos");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3(("##Pos" + ptrId).c_str(), &tr.position.x, 0.01f);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Rot");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3(("##Rot" + ptrId).c_str(), &tr.rotation.x, 0.01f);
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Scl");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3(("##Scl" + ptrId).c_str(), &tr.scale.x, 0.01f, 0.001f, 1000.0f);
            ImGui::EndTable();
        }
        _Parent->SetTransform(tr);
    }

    // モデルアセット一覧
    if (ImGui::BeginTable(("ModelAssetTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Filter");
        ImGui::TableSetColumnIndex(1); ImGui::InputText(("##Filter" + ptrId).c_str(), m_modelSearch, sizeof(m_modelSearch));

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
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Asset");
        ImGui::TableSetColumnIndex(1);
        {
            ImGui::BeginChild(("##ModelList" + ptrId).c_str(), ImVec2(0, 120), true);
            for (int i = 0; i < (int)models.size(); ++i) {
                bool sel = (i == selectedIdx);
                if (ImGui::Selectable((models[i] + "##M" + ptrId + std::to_string(i)).c_str(), sel)) {
                    selectedIdx = i;
                    strncpy_s(m_modelAssetInput, models[i].c_str(), _TRUNCATE);
                }
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                    if (selectedIdx >= 0) LoadModelFromAsset(models[selectedIdx], true);
                }
            }
            ImGui::EndChild();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Manual");
        ImGui::TableSetColumnIndex(1); ImGui::InputText(("##ManualAsset" + ptrId).c_str(), m_modelAssetInput, sizeof(m_modelAssetInput));

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Ops");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button(("Load##" + ptrId).c_str())) {
            if (m_modelAssetInput[0]) LoadModelFromAsset(m_modelAssetInput, true);
        } ImGui::SameLine();
        if (ImGui::Button(("Reload##" + ptrId).c_str())) {
            if (!m_currentModelAsset.empty()) LoadModelFromAsset(m_currentModelAsset, true);
        } ImGui::SameLine();
        if (ImGui::Button(("Clear##" + ptrId).c_str())) {
            ClearModel();
        }

        if (m_modelLoaded) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Current");
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_currentModelAsset.c_str());
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Stats");
            ImGui::TableSetColumnIndex(1); ImGui::Text("V:%zu I:%zu Sub:%zu", m_vertices.size(), m_indices.size(), m_subMeshes.size());
        }
        else {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Status");
            ImGui::TableSetColumnIndex(1); ImGui::Text("Not Loaded");
        }
        ImGui::EndTable();
    }

    // 統一シェーダ設定 (選択リセットバグ対策)
    if (ImGui::BeginTable(("UnifiedShaderTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
        auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
        static int  vsSel = -1, psSel = -1;
        static bool initVS = false, initPS = false;

        // 初期化時 or リスト変更時のみ同期
        if (!initVS) {
            for (int i = 0; i < (int)vsList.size(); ++i) if (vsList[i] == m_unifiedVS) { vsSel = i; break; }
            initVS = true;
        }
        if (!initPS) {
            for (int i = 0; i < (int)psList.size(); ++i) if (psList[i] == m_unifiedPS) { psSel = i; break; }
            initPS = true;
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Unified VS");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginCombo(("##UniVS" + ptrId).c_str(), (vsSel >= 0 && vsSel < (int)vsList.size()) ? vsList[vsSel].c_str() : "-")) {
            for (int i = 0; i < (int)vsList.size(); ++i) {
                bool sel = (i == vsSel);
                if (ImGui::Selectable((vsList[i] + "##UVS" + ptrId + std::to_string(i)).c_str(), sel))
                    vsSel = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Unified PS");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::BeginCombo(("##UniPS" + ptrId).c_str(), (psSel >= 0 && psSel < (int)psList.size()) ? psList[psSel].c_str() : "-")) {
            for (int i = 0; i < (int)psList.size(); ++i) {
                bool sel = (i == psSel);
                if (ImGui::Selectable((psList[i] + "##UPS" + ptrId + std::to_string(i)).c_str(), sel))
                    psSel = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Apply");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button(("Apply Unified##" + ptrId).c_str())) {
            if (vsSel >= 0 && vsSel < (int)vsList.size() && psSel >= 0 && psSel < (int)psList.size()) {
                SetUnifiedShader(vsList[vsSel], psList[psSel]);
                // 再同期
                m_unifiedVS = vsList[vsSel];
                m_unifiedPS = psList[psSel];
            }
        }

        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Skinning");
        ImGui::TableSetColumnIndex(1); ImGui::Checkbox(("##SkinEnable" + ptrId).c_str(), &m_enableSkinning);

        ImGui::EndTable();
    }

    // 個別マテリアル
    if (!m_materials.empty()) {
        if (ImGui::BeginTable(("MaterialTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Index");
            ImGui::TableSetColumnIndex(1); ImGui::DragInt(("##MatIndex" + ptrId).c_str(), &m_selectedMaterial, 1, 0, (int)m_materials.size() - 1);
            m_selectedMaterial = std::clamp(m_selectedMaterial, 0, (int)m_materials.size() - 1);
            auto& mat = m_materials[m_selectedMaterial];

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Override");
            ImGui::TableSetColumnIndex(1); ImGui::Checkbox(("##MatOv" + ptrId).c_str(), &mat.overrideUnified);

            auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
            auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
            // 選択初期化用
            static int vsLocal = -1, psLocal = -1;
            static int lastMatIndex = -1;
            if (lastMatIndex != m_selectedMaterial) {
                vsLocal = psLocal = -1;
                for (int i = 0; i < (int)vsList.size(); ++i) if (vsList[i] == mat.vertexShader) { vsLocal = i; break; }
                for (int i = 0; i < (int)psList.size(); ++i) if (psList[i] == mat.pixelShader) { psLocal = i; break; }
                lastMatIndex = m_selectedMaterial;
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Mat VS");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo(("##MatVS" + ptrId).c_str(), (vsLocal >= 0 && vsLocal < (int)vsList.size()) ? vsList[vsLocal].c_str() : "-")) {
                for (int i = 0; i < (int)vsList.size(); ++i) {
                    bool sel = (i == vsLocal);
                    if (ImGui::Selectable((vsList[i] + "##MatVSItem" + ptrId + std::to_string(i)).c_str(), sel))
                        vsLocal = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Mat PS");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo(("##MatPS" + ptrId).c_str(), (psLocal >= 0 && psLocal < (int)psList.size()) ? psList[psLocal].c_str() : "-")) {
                for (int i = 0; i < (int)psList.size(); ++i) {
                    bool sel = (i == psLocal);
                    if (ImGui::Selectable((psList[i] + "##MatPSItem" + ptrId + std::to_string(i)).c_str(), sel))
                        psLocal = i;
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Apply");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button(("Apply Mat Shader##" + ptrId).c_str())) {
                if (vsLocal >= 0 && vsLocal < (int)vsList.size() && psLocal >= 0 && psLocal < (int)psList.size()) {
                    SetMaterialShader((uint32_t)m_selectedMaterial, vsList[vsLocal], psList[psLocal], true);
                }
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("BaseColor");
            ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4(("##MatCol" + ptrId).c_str(), &mat.baseColor.x);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Metallic");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat(("##MatMetal" + ptrId).c_str(), &mat.metallic, 0.01f, 0.0f, 1.0f);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Roughness");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat(("##MatRough" + ptrId).c_str(), &mat.roughness, 0.01f, 0.0f, 1.0f);

            // テクスチャ読み込み
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Tex Name");
            ImGui::TableSetColumnIndex(1);
            ImGui::InputText(("##NewTex" + ptrId).c_str(), m_newTextureName, sizeof(m_newTextureName));
            static int texSlot = 0;
            ImGui::DragInt(("##TexSlot" + ptrId).c_str(), &texSlot, 1, 0, 32);
            if (ImGui::Button(("LoadTex##" + ptrId).c_str())) {
                if (m_newTextureName[0])
                    SetMaterialTexture((uint32_t)m_selectedMaterial, (uint32_t)texSlot, m_newTextureName);
            }

            // テクスチャ一覧
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Textures");
            ImGui::TableSetColumnIndex(1);
            if (mat.textures.empty()) {
                ImGui::Text("(none)");
            }
            else {
                for (size_t i = 0; i < mat.textures.size(); ++i) {
                    if (!mat.textures[i].assetName.empty())
                        ImGui::Text("[%zu] %s%s", i, mat.textures[i].assetName.c_str(), (i == 0 && mat.textures[i].srv) ? " (Base)" : "");
                }
            }

            ImGui::EndTable();
        }
    }

    // アニメーション
    if (ImGui::BeginTable(("AnimTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        if (m_clips.empty()) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Clips");
            ImGui::TableSetColumnIndex(1); ImGui::Text("None");
        }
        else {
            if (m_selectedAnimIndex < 0 || m_selectedAnimIndex >= (int)m_clips.size()) m_selectedAnimIndex = 0;
            std::string cur = m_clips[m_selectedAnimIndex].name;
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Clip");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo(("##AnimSel" + ptrId).c_str(), cur.c_str())) {
                for (int i = 0; i < (int)m_clips.size(); ++i) {
                    bool sel = (i == m_selectedAnimIndex);
                    if (ImGui::Selectable((m_clips[i].name + "##A" + ptrId + std::to_string(i)).c_str(), sel)) {
                        m_selectedAnimIndex = i;
                        PlayAnimation(m_clips[i].name, m_loop, (float)m_animSpeed);
                    }
                    if (sel) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Loop");
            ImGui::TableSetColumnIndex(1); ImGui::Checkbox(("##Loop" + ptrId).c_str(), &m_loop);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Speed");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat(("##Spd" + ptrId).c_str(), (float*)&m_animSpeed, 0.01f, 0.0f, 10.0f);

            if (m_currentClip >= 0) {
                auto& clip = m_clips[m_currentClip];
                float curT = (float)m_animTime;
                float maxT = (float)clip.duration;
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Time");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::SliderFloat(("##Time" + ptrId).c_str(), &curT, 0.0f, maxT)) {
                    m_animTime = curT;
                    ApplyAnimationToBones(m_animTime);
                    UpdateBoneMatrices();
                }
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Ctrl");
                ImGui::TableSetColumnIndex(1);
                if (ImGui::Button(("Restart##" + ptrId).c_str())) m_animTime = 0.0;
                ImGui::SameLine();
                if (ImGui::Button(("Stop##" + ptrId).c_str())) StopAnimation();
            }
        }
        ImGui::EndTable();
    }

    // IK
    if (ImGui::BeginTable(("IKTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Chain");
        ImGui::TableSetColumnIndex(1); ImGui::InputText(("##IKChain" + ptrId).c_str(), m_ikChainInput, sizeof(m_ikChainInput));
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Target");
        ImGui::TableSetColumnIndex(1); ImGui::DragFloat3(("##IKTarget" + ptrId).c_str(), &m_ikTarget.x, 0.01f);
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Iter");
        ImGui::TableSetColumnIndex(1); ImGui::DragInt(("##IKIter" + ptrId).c_str(), &m_ikIterations, 1, 1, 200);
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Thresh");
        ImGui::TableSetColumnIndex(1); ImGui::DragFloat(("##IKTh" + ptrId).c_str(), &m_ikThresholdDeg, 0.01f, 0.0f, 20.0f);
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Solve");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button(("Solve##" + ptrId).c_str())) {
            std::vector<int> chain;
            std::stringstream ss(m_ikChainInput);
            std::string seg;
            while (std::getline(ss, seg, ',')) {
                if (!seg.empty()) chain.push_back(std::atoi(seg.c_str()));
            }
            if (chain.size() >= 2) SolveIK_CCD(chain, m_ikTarget, m_ikIterations, m_ikThresholdDeg);
        }
        ImGui::EndTable();
    }

    // 情報
    if (ImGui::BeginTable(("InfoTable" + ptrId).c_str(), 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Loaded");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%s", m_currentModelAsset.empty() ? "(None)" : m_currentModelAsset.c_str());
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Bones");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%zu", m_bones.size());
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Clips");
        ImGui::TableSetColumnIndex(1); ImGui::Text("%zu", m_clips.size());
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("CurClip");
        ImGui::TableSetColumnIndex(1);
        if (m_currentClip >= 0) ImGui::Text("%s", m_clips[m_currentClip].name.c_str());
        else ImGui::Text("-");
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Mat0 Tex0");
        ImGui::TableSetColumnIndex(1);
        if (!m_materials.empty() && !m_materials[0].textures.empty() && m_materials[0].textures[0].srv)
            ImGui::Text("OK (%s)", m_materials[0].textures[0].assetName.c_str());
        else
            ImGui::Text("None");
        ImGui::EndTable();
    }
}