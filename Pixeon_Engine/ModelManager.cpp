
#include "ModelManager.h"
#include "AssetManager.h"
#include "System.h"
#include "ErrorLog.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <Windows.h>
#include "IMGUI/imgui.h"
#include <filesystem>
#include <unordered_set>

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

static std::string MM_NormalizePath(std::string s) {
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() && (s[0] == '/' || (s.size() >= 2 && s[0] == '.' && s[1] == '/'))) {
        if (s[0] == '/') s.erase(0, 1);
        else if (s.rfind("./", 0) == 0) s.erase(0, 2);
        else break;
    }
    return s;
}

ModelManager* ModelManager::s_instance = nullptr;

ModelManager* ModelManager::Instance() {
    if (!s_instance) {
        s_instance = new ModelManager();
    }
    return s_instance;
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
    if (!AssetManager::Instance()->LoadAsset(logicalName, data) || data.empty()) {
		ErrorLogger::Instance().LogError("ModelManager", "Failed to load model asset: " + logicalName);
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
        ErrorLogger::Instance().LogError("ModelManager", "Assimp parse failed: " + logicalName + 
			(importer.GetErrorString()[0] ? (" (" + std::string(importer.GetErrorString()) + ")") : ""));
        return nullptr;
    }

    auto shared = std::make_shared<ModelSharedResource>();
    shared->source = logicalName;

    // ���_�E�C���f�b�N�X�f�[�^�̏���
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;

    ProcessNode(scene->mRootNode, scene, vertices, indices, shared);

    // GPU�o�b�t�@�̍쐬
    if (!CreateGPUBuffers(vertices, indices, shared)) {
		ErrorLogger::Instance().LogError("ModelManager", "GPU buffer creation failed: " + logicalName);
        return nullptr;
    }

    // �}�e���A�����̏���
    ProcessMaterials(scene, shared);

    // �{�[�����̏���
    ProcessBones(scene, shared);

    // �A�j���[�V�������̏���
    ProcessAnimations(scene, shared);

    // �������g�p�ʂ̌v�Z
    shared->gpuBytes = vertices.size() * sizeof(ModelVertex) + indices.size() * sizeof(uint32_t);

	//ErrorLogger::Instance().LogError("ModelManager", "Load OK: " + logicalName, false, 5);
    return shared;
}

std::string ModelManager::ResolveTexturePath(const std::string& modelLogical, const std::string& rawPath){
    if (rawPath.empty()) return {};
    if (rawPath[0] == '*') { // ���ߍ��ݖ��Ή�
		ErrorLogger::Instance().LogError("ModelManager", "Embedded texture unsupported: " + rawPath);
        return {};
    }
    std::string norm = MM_NormalizePath(rawPath);

    // ��΃p�X -> �t�@�C�����̂�
    {
        std::filesystem::path p(norm);
        if (p.is_absolute()) norm = p.filename().generic_string();
    }

    // ���f���̃f�B���N�g��
    std::string modelDir;
    if (auto pos = modelLogical.find_last_of("/\\"); pos != std::string::npos) {
        modelDir = modelLogical.substr(0, pos + 1);
        for (auto& c : modelDir) if (c == '\\') c = '/';
    }
    std::string filename = norm;
    if (auto pos = norm.find_last_of('/'); pos != std::string::npos)
        filename = norm.substr(pos + 1);

    std::vector<std::string> candidates;
    if (norm.find('/') != std::string::npos) candidates.push_back(norm);
    candidates.push_back(modelDir + norm);
    candidates.push_back(modelDir + filename);
    candidates.push_back(modelDir + "Textures/" + filename);
    candidates.push_back("Textures/" + filename);
    candidates.push_back(filename);

    std::unordered_set<std::string> seen;
    std::vector<std::string> uniq;
    for (auto& c : candidates) {
        auto n = MM_NormalizePath(c);
        if (seen.insert(n).second) uniq.push_back(n);
    }
    for (auto& c : uniq) {
        if (AssetManager::Instance()->Exists(c)) {
            return c;
        }
    }
	ErrorLogger::Instance().LogError("ModelManager", "Texture not found: " + rawPath + " (tried " + std::to_string(uniq.size()) + " paths)", false, 3);
    return {};
}

void ModelManager::ProcessNode(aiNode* node, const aiScene* scene,
    std::vector<ModelVertex>& vertices,
    std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    // ���b�V���̏���
    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene, vertices, indices, shared);
    }

    // �q�m�[�h�̏���
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

    // ���_�f�[�^�̏���
    for (uint32_t i = 0; i < mesh->mNumVertices; i++) {
        ModelVertex vertex = {};

        // �ʒu
        vertex.position[0] = mesh->mVertices[i].x;
        vertex.position[1] = mesh->mVertices[i].y;
        vertex.position[2] = mesh->mVertices[i].z;

        // �@��
        if (mesh->HasNormals()) {
            vertex.normal[0] = mesh->mNormals[i].x;
            vertex.normal[1] = mesh->mNormals[i].y;
            vertex.normal[2] = mesh->mNormals[i].z;
        }

        // �^���W�F���g
        if (mesh->mTangents) {
            vertex.tangent[0] = mesh->mTangents[i].x;
            vertex.tangent[1] = mesh->mTangents[i].y;
            vertex.tangent[2] = mesh->mTangents[i].z;
            vertex.tangent[3] = 1.0f;
        }

        // UV���W
        if (mesh->mTextureCoords[0]) {
            vertex.uv[0] = mesh->mTextureCoords[0][i].x;
            vertex.uv[1] = mesh->mTextureCoords[0][i].y;
        }

        // �{�[���̃E�F�C�g�i�������j
        for (int j = 0; j < 4; j++) {
            vertex.boneIndices[j] = 0;
            vertex.boneWeights[j] = 0.0f;
        }

        vertices.push_back(vertex);
    }

    // �C���f�b�N�X�f�[�^�̏���
    for (uint32_t i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (uint32_t j = 0; j < face.mNumIndices; j++) {
            indices.push_back(vertexOffset + face.mIndices[j]);
        }
    }

    {
        // UV ���S�� 0 �̏ꍇ���m
        bool anyUV = false;
        bool allZero = true;
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            if (mesh->mTextureCoords[0]) {
                anyUV = true;
                if (!(mesh->mTextureCoords[0][i].x == 0.0f && mesh->mTextureCoords[0][i].y == 0.0f)) {
                    allZero = false;
                    break;
                }
            }
        }
        if (!anyUV) {
            char buf[128];
            sprintf_s(buf, "[ModelManager] Mesh mat=%u UV channel MISSING\n", mesh->mMaterialIndex);
			ErrorLogger::Instance().LogError("ModelManager", buf, false, 3);
        }
        else if (allZero) {
            char buf[128];
            sprintf_s(buf, "[ModelManager] Mesh mat=%u UV ALL ZERO\n", mesh->mMaterialIndex);
			ErrorLogger::Instance().LogError("ModelManager", buf, false, 3);
        }
    }

    bool anyUV = false;
    bool allZero = true;
    if (mesh->mTextureCoords[0]) {
        anyUV = true;
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i) {
            float ux = mesh->mTextureCoords[0][i].x;
            float uy = mesh->mTextureCoords[0][i].y;
            if (!(ux == 0.0f && uy == 0.0f)) {
                allZero = false;
                break;
            }
        }
    }
    subMesh.hasUV = anyUV;
    subMesh.uvAllZero = anyUV ? allZero : false;

    // ������ ErrorLogger �Ăяo���͂��̂܂�/�܂��͈ȉ��̂悤�ɐ���
    if (!anyUV) {
        ErrorLogger::Instance().LogError("ModelManager",
            "[Mesh mat=" + std::to_string(mesh->mMaterialIndex) + "] UV channel MISSING", false, 3);
    }
    else if (allZero) {
        ErrorLogger::Instance().LogError("ModelManager",
            "[Mesh mat=" + std::to_string(mesh->mMaterialIndex) + "] UV ALL ZERO", false, 3);
    }


    subMesh.indexCount = static_cast<uint32_t>(indices.size()) - subMesh.indexOffset;
    shared->submeshes.push_back(subMesh);
}

bool ModelManager::CreateGPUBuffers(const std::vector<ModelVertex>& vertices,
    const std::vector<uint32_t>& indices,
    std::shared_ptr<ModelSharedResource> shared) {

    auto device = DirectX11::GetInstance()->GetDevice();
    if (!device) return false;

    // ���_�o�b�t�@�̍쐬
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(ModelVertex));
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, shared->vb.GetAddressOf());
    if (FAILED(hr)) return false;

    // �C���f�b�N�X�o�b�t�@�̍쐬
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

        aiColor4D color;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_DIFFUSE, color)) {
            material.baseColor = DirectX::XMFLOAT4(color.r, color.g, color.b, color.a);
        }

        float metallic, roughness;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic)) material.metallic = metallic;
        if (AI_SUCCESS == mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness)) material.roughness = roughness;

        aiString texPath;
        // 複数のテクスチャタイプを優先順で試行 (DIFFUSE → BASE_COLOR → EMISSIVE → AMBIENT)
        aiTextureType texTypes[] = { 
            aiTextureType_DIFFUSE, 
            aiTextureType_BASE_COLOR, 
            aiTextureType_EMISSIVE, 
            aiTextureType_AMBIENT 
        };
        
        bool foundTexture = false;
        for (aiTextureType texType : texTypes) {
            if (AI_SUCCESS == mat->GetTexture(texType, 0, &texPath)) {
                std::string resolved = ResolveTexturePath(shared->source, texPath.C_Str());
                material.baseColorTex = resolved;
                foundTexture = true;
                // デバッグ用: どのテクスチャタイプで見つかったかログ出力
                OutputDebugStringA(("[ModelManager] Material " + std::to_string(i) + 
                    " texture found in type " + std::to_string(texType) + 
                    ": " + resolved + "\n").c_str());
                break; // 最初に見つかったテクスチャを使用
            }
        }
        
        if (!foundTexture) {
            OutputDebugStringA(("[ModelManager] Material " + std::to_string(i) + 
                " has no texture in any supported type\n").c_str());
        }

        shared->materials.push_back(material);
    }
}

void ModelManager::ProcessBones(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    // �{�[�����̏����i�ȈՎ����j
    // ���ۂ̃v���W�F�N�g�ł́A���ڍׂȎ������K�v
    shared->hasSkin = false;
    for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
        if (scene->mMeshes[i]->HasBones()) {
            shared->hasSkin = true;
            break;
        }
    }
}

void ModelManager::ProcessAnimations(const aiScene* scene, std::shared_ptr<ModelSharedResource> shared) {
    // �A�j���[�V�������̏����i�ȈՎ����j
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

void ModelManager::DrawDebugGUI() {
    std::lock_guard<std::mutex> lk(m_mtx);
    ImGui::TextUnformatted("ModelManager");
    ImGui::Separator();
    size_t alive = 0;
    size_t totalGPU = 0;
    for (auto& kv : m_cache) {
        if (!kv.second.weak.expired()) {
            alive++;
            totalGPU += kv.second.gpuBytes;
        }
    }
    ImGui::Text("Cached: %zu (alive=%zu)", m_cache.size(), alive);
    ImGui::Text("GPU Approx Total: %.2f MB", totalGPU / (1024.0 * 1024.0));
    static char filter[128] = "";
    ImGui::InputText("Filter##Model", filter, sizeof(filter));
    if (ImGui::Button("GC Dead")) {
        for (auto it = m_cache.begin(); it != m_cache.end();) {
            if (it->second.weak.expired()) it = m_cache.erase(it);
            else ++it;
        }
    }
    ImGui::Separator();
    ImGui::BeginChild("ModelList", ImVec2(0, 160), true);
    for (auto& kv : m_cache) {
        if (filter[0] && kv.first.find(filter) == std::string::npos) continue;
        bool aliveRes = !kv.second.weak.expired();
        ImGui::Text("%s | %s | %.2f KB | lastUse=%llu",
            kv.first.c_str(),
            aliveRes ? "alive" : "dead",
            kv.second.gpuBytes / 1024.0,
            (unsigned long long)kv.second.lastUse);
    }
    ImGui::EndChild();
}