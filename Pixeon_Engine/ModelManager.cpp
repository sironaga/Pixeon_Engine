
#include "ModelManager.h"
#include "AssetManager.h"
#include "System.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>

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

ModelManager* ModelManager::s_instance = nullptr;

ModelManager& ModelManager::Instance() {
    if (!s_instance) {
        s_instance = new ModelManager();
    }
    return *s_instance;
}

std::shared_ptr<ModelSharedResource> ModelManager::LoadOrGet(const std::string& logicalName) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_frame++;

    auto it = m_cache.find(logicalName);
    if (it != m_cache.end()) {
        if (auto sp = it->second.weak.lock()) {
            it->second.lastUse = m_frame;
            return sp;
        }
    }

    auto res = LoadInternal(logicalName);
    if (res) {
        Entry e;
        e.weak = res;
        e.lastUse = m_frame;
        e.gpuBytes = res->gpuBytes;
        m_cache[logicalName] = e;
    }
    return res;
}

std::shared_ptr<ModelSharedResource> ModelManager::LoadInternal(const std::string& logicalName) {
    std::vector<uint8_t> data;
    if (!AssetManager::Instance().LoadAsset(logicalName, data) || data.empty()) {
        OutputDebugStringA(("[ModelManager] Raw load failed: " + logicalName + "\n").c_str());
        return nullptr;
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
        aiProcess_SortByPType);

    if (!scene || !scene->mRootNode) {
        OutputDebugStringA(("[ModelManager] Assimp parse failed: " + logicalName + "\n").c_str());
        return nullptr;
    }

    auto shared = std::make_shared<ModelSharedResource>();
    shared->source = logicalName;

    // 頂点・インデックスデータの処理
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;

    ProcessNode(scene->mRootNode, scene, vertices, indices, shared);

    // GPUバッファの作成
    if (!CreateGPUBuffers(vertices, indices, shared)) {
        OutputDebugStringA(("[ModelManager] GPU buffer creation failed: " + logicalName + "\n").c_str());
        return nullptr;
    }

    // マテリアル情報の処理
    ProcessMaterials(scene, shared);

    // ボーン情報の処理
    ProcessBones(scene, shared);

    // アニメーション情報の処理
    ProcessAnimations(scene, shared);

    // メモリ使用量の計算
    shared->gpuBytes = vertices.size() * sizeof(ModelVertex) + indices.size() * sizeof(uint32_t);

    OutputDebugStringA(("[ModelManager] Load OK: " + logicalName + "\n").c_str());
    return shared;
}

void ModelManager::ProcessNode(aiNode* node, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    // メッシュの処理
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, vertices, indices, shared);
    }

    // 子ノードの処理
    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, vertices, indices, shared);
    }
}

void ModelManager::ProcessMesh(aiMesh* mesh, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    SubMesh subMesh;
    subMesh.indexOffset = static_cast<uint32_t>(indices.size());
    subMesh.materialIndex = mesh->mMaterialIndex;
    subMesh.skinned = mesh->HasBones();

    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

    // 頂点データの処理
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        ModelVertex vertex = {};

        // 位置
        vertex.position[0] = mesh->mVertices[i].x;
        vertex.position[1] = mesh->mVertices[i].y;
        vertex.position[2] = mesh->mVertices[i].z;

        // 法線
        if (mesh->HasNormals()) {
            vertex.normal[0] = mesh->mNormals[i].x;
            vertex.normal[1] = mesh->mNormals[i].y;
            vertex.normal[2] = mesh->mNormals[i].z;
        }

        // タンジェント
        if (mesh->mTangents) {
            vertex.tangent[0] = mesh->mTangents[i].x;
            vertex.tangent[1] = mesh->mTangents[i].y;
            vertex.tangent[2] = mesh->mTangents[i].z;
            vertex.tangent[3] = 1.0f;
        }

        // UV座標
        if (mesh->mTextureCoords[0]) {
            vertex.uv[0] = mesh->mTextureCoords[0][i].x;
            vertex.uv[1] = mesh->mTextureCoords[0][i].y;
        }

        // ボーンのウェイト（初期化）
        for (int j = 0; j < 4; j++) {
            vertex.boneIndices[j] = 0;
            vertex.boneWeights[j] = 0.0f;
        }

        vertices.push_back(vertex);
    }

    // インデックスデータの処理
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(vertexOffset + face.mIndices[j]);
        }
    }

    subMesh.indexCount = static_cast<uint32_t>(indices.size()) - subMesh.indexOffset;
    shared->submeshes.push_back(subMesh);
}

bool ModelManager::CreateGPUBuffers(const std::vector<ModelVertex>& vertices,
    const std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    auto device = DirectX11::GetInstance()->GetDevice();
    if (!device) return false;

    // 頂点バッファの作成
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(ModelVertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, shared->vb.GetAddressOf());
    if (FAILED(hr)) return false;

    // インデックスバッファの作成
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(uint32_t));
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibDesc, &ibData, shared->ib.GetAddressOf());
    if (FAILED(hr)) return false;

    shared->vertexCount = static_cast<uint32_t>(vertices.size());
    shared->indexCount = static_cast<uint32_t>(indices.size());

    return true;
}

void ModelManager::ProcessMaterials(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* mat = scene->mMaterials[i];
        MaterialShared material;

        // ベースカラーの取得
        aiColor4D color;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
            material.baseColor = DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
        }

        // メタリック・ラフネスの取得（可能な場合）
        float metallic, roughness;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) {
            material.metallic = metallic;
        }
        if (AI_SUCCESS == mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) {
            material.roughness = roughness;
        }

        // テクスチャパスの取得
        aiString texPath;
        if (AI_SUCCESS == mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            material.baseColorTex = texPath.C_Str();
        }

        shared->materials.push_back(material);
    }
}

void ModelManager::ProcessBones(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    // ボーン情報の処理（簡易実装）
    // 実際のプロジェクトでは、より詳細な実装が必要
    shared->hasSkin = false;
    for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
        if (scene->mMeshes[i]->HasBones()) {
            shared->hasSkin = true;
            break;
        }
    }
}

void ModelManager::ProcessAnimations(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    // アニメーション情報の処理（簡易実装）
    for (uint32_t i = 0; i < scene->mNumAnimations; i++) {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationClip clip;
        clip.name = anim->mName.length > 0 ? anim->mName.C_Str() : ("Animation_" + std::to_string(i));
        clip.duration = anim->mDuration;
        clip.tps = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;

        shared->clips.push_back(clip);
    }
}

void ModelManager::GarbageCollect() {
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