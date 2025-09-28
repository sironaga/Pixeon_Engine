#include "ModelManager.h"
#include "AssetManager.h"
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

ModelManager& ModelManager::Instance()
{
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
    // ���ȉ��̎������K�v��
     // 1. ���_�f�[�^�̕ϊ��E�i�[
     // 2. �C���f�b�N�X�f�[�^�̕ϊ��E�i�[  
     // 3. GPU �o�b�t�@�̍쐬 (CreateBuffer)
     // 4. �}�e���A�����̒��o�E�i�[
     // 5. �{�[�����̒��o�E�i�[
     // 6. �A�j���[�V�������̒��o�E�i�[
     // 7. GPU�g�p�ʂ̌v�Z
    return shared;
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
