#include "ModelRuntimeResource.h"
#include "ShaderManager.h"
#include "DirectXTex/DirectXTex.h"
#include <filesystem>
#include <algorithm>

using namespace DirectX;

static bool IsImageExt(const std::string& ext) {
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), ::tolower);
    return (e == "png" || e == "jpg" || e == "jpeg" || e == "bmp" || e == "tga" || e == "dds" || e == "hdr");
}

DirectX::XMMATRIX ModelRuntimeResource::ToXM(const aiMatrix4x4& m) {
    return XMMATRIX(
        (float)m.a1, (float)m.b1, (float)m.c1, (float)m.d1,
        (float)m.a2, (float)m.b2, (float)m.c2, (float)m.d2,
        (float)m.a3, (float)m.b3, (float)m.c3, (float)m.d3,
        (float)m.a4, (float)m.b4, (float)m.c4, (float)m.d4
    );
}

void ModelRuntimeResource::ExtractColors(aiMaterial* aimat, AModelMaterial& out) {
    aiColor4D c;
#ifdef AI_MATKEY_BASE_COLOR
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_BASE_COLOR, c)) {
        out.baseColor = { c.r,c.g,c.b,c.a };
    }
    else
#endif
        if (AI_SUCCESS == aimat->Get(AI_MATKEY_COLOR_DIFFUSE, c)) {
            out.baseColor = { c.r,c.g,c.b,c.a };
        }
#ifdef AI_MATKEY_METALLIC_FACTOR
    float m = 0;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_METALLIC_FACTOR, m)) out.metallic = m;
#endif
#ifdef AI_MATKEY_ROUGHNESS_FACTOR
    float r = 0;
    if (AI_SUCCESS == aimat->Get(AI_MATKEY_ROUGHNESS_FACTOR, r)) out.roughness = r;
#endif
}

bool ModelRuntimeResource::LoadFromAsset(const std::string& assetName) {
    vertices.clear(); indices.clear();
    submeshes.clear(); materials.clear();
    bones.clear(); animations.clear();
    sourceAssetName.clear();
    hasSkin = false;

    std::vector<uint8_t> bin;
    if (!AssetsManager::GetInstance()->LoadAsset(assetName, bin)) {
        OutputDebugStringA(("[AModel] LoadAsset failed: " + assetName + "\n").c_str());
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
        aiProcess_SortByPType
    );
    if (!scene || !scene->mRootNode) {
        OutputDebugStringA("[AModel] Assimp parse failed\n");
        return false;
    }
    if (!ImportScene(assetName, scene)) {
        OutputDebugStringA("[AModel] ImportScene failed\n");
        return false;
    }
    sourceAssetName = assetName;
    for (uint32_t i = 0; i < AMODEL_MAX_BONES; i++) finalBonePalette[i] = XMMatrixIdentity();
    return true;
}

bool ModelRuntimeResource::ImportScene(const std::string& assetName, const aiScene* scene) {
    materials.resize(scene->mNumMaterials);
    std::string baseDir;
    if (auto p = assetName.find_last_of("/\\"); p != std::string::npos)
        baseDir = assetName.substr(0, p);

    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* aimat = scene->mMaterials[i];
        AModelMaterial mm;
        ExtractColors(aimat, mm);
        mm.vs = unifiedVS;
        mm.ps = unifiedPS;

        aiString pth;
#ifdef AI_MATKEY_BASE_COLOR_TEXTURE
        if (aimat->GetTextureCount(aiTextureType_BASE_COLOR) > 0 &&
            aimat->GetTexture(aiTextureType_BASE_COLOR, 0, &pth) == AI_SUCCESS) {
            mm.textures.resize(1);
            mm.textures[0].originalPath = pth.C_Str();
        }
        else
#endif
            if (aimat->GetTexture(aiTextureType_DIFFUSE, 0, &pth) == AI_SUCCESS) {
                mm.textures.resize(1);
                mm.textures[0].originalPath = pth.C_Str();
            }
        materials[i] = std::move(mm);
    }

    ProcessNode(scene->mRootNode, scene, -1);

    if (scene->mNumAnimations > 0) {
        animations.reserve(scene->mNumAnimations);
        for (uint32_t a = 0; a < scene->mNumAnimations; a++) {
            aiAnimation* aa = scene->mAnimations[a];
            AModelAnimation clip;
            clip.name = aa->mName.length ? aa->mName.C_Str() : ("Anim" + std::to_string(a));
            clip.duration = aa->mDuration;
            clip.ticksPerSecond = (aa->mTicksPerSecond != 0.0) ? aa->mTicksPerSecond : 25.0;
            for (uint32_t c = 0; c < aa->mNumChannels; c++) {
                aiNodeAnim* ch = aa->mChannels[c];
                AModelBoneChannel bc; bc.boneName = ch->mNodeName.C_Str();
                for (uint32_t k = 0; k < ch->mNumPositionKeys; k++) {
                    AModelKeyFrame kf; kf.time = ch->mPositionKeys[k].mTime;
                    kf.t = { (float)ch->mPositionKeys[k].mValue.x,(float)ch->mPositionKeys[k].mValue.y,(float)ch->mPositionKeys[k].mValue.z };
                    bc.translations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumRotationKeys; k++) {
                    AModelKeyFrame kf; kf.time = ch->mRotationKeys[k].mTime;
                    kf.r = { (float)ch->mRotationKeys[k].mValue.x,(float)ch->mRotationKeys[k].mValue.y,(float)ch->mRotationKeys[k].mValue.z,(float)ch->mRotationKeys[k].mValue.w };
                    bc.rotations.push_back(kf);
                }
                for (uint32_t k = 0; k < ch->mNumScalingKeys; k++) {
                    AModelKeyFrame kf; kf.time = ch->mScalingKeys[k].mTime;
                    kf.s = { (float)ch->mScalingKeys[k].mValue.x,(float)ch->mScalingKeys[k].mValue.y,(float)ch->mScalingKeys[k].mValue.z };
                    bc.scalings.push_back(kf);
                }
                clip.channels.push_back(std::move(bc));
            }
            animations.push_back(std::move(clip));
        }
    }
    return true;
}

void ModelRuntimeResource::ProcessNode(aiNode* node, const aiScene* scene, int parent) {
    int myIndex = (int)bones.size();
    AModelBone b;
    b.name = node->mName.C_Str();
    b.parent = parent;
    b.offset = XMMatrixIdentity();
    b.local = ToXM(node->mTransformation);
    b.global = XMMatrixIdentity();
    bones.push_back(b);

    for (uint32_t m = 0; m < node->mNumMeshes; m++) {
        ProcessMesh(scene->mMeshes[node->mMeshes[m]], scene);
    }
    for (uint32_t c = 0; c < node->mNumChildren; c++) {
        ProcessNode(node->mChildren[c], scene, myIndex);
    }
}

void ModelRuntimeResource::ProcessMesh(aiMesh* mesh, const aiScene*) {
    AModelSubMesh sm;
    sm.materialIndex = mesh->mMaterialIndex;
    sm.vertexOffset = (uint32_t)vertices.size();
    sm.indexOffset = (uint32_t)indices.size();
    sm.skinned = mesh->HasBones();
    sm.name = mesh->mName.C_Str();

    for (uint32_t v = 0; v < mesh->mNumVertices; v++) {
        AModelVertex vert{};
        vert.position = { (float)mesh->mVertices[v].x,(float)mesh->mVertices[v].y,(float)mesh->mVertices[v].z };
        vert.normal = mesh->HasNormals() ? XMFLOAT3((float)mesh->mNormals[v].x, (float)mesh->mNormals[v].y, (float)mesh->mNormals[v].z) : XMFLOAT3(0, 1, 0);
        vert.tangent = mesh->mTangents ? XMFLOAT4((float)mesh->mTangents[v].x, (float)mesh->mTangents[v].y, (float)mesh->mTangents[v].z, 1) : XMFLOAT4(1, 0, 0, 1);
        vert.uv = mesh->mTextureCoords[0] ? XMFLOAT2((float)mesh->mTextureCoords[0][v].x, (float)mesh->mTextureCoords[0][v].y) : XMFLOAT2(0, 0);
        if (!mesh->HasBones()) {
            vert.boneIndices[0] = 0;
            vert.boneWeights[0] = 1.0f;
        }
        vertices.push_back(vert);
    }

    if (mesh->HasBones()) {
        hasSkin = true;
        for (uint32_t b = 0; b < mesh->mNumBones; b++) {
            aiBone* aib = mesh->mBones[b];
            int boneIndex = -1;
            for (int bi = 0; bi < (int)bones.size(); bi++) {
                if (bones[bi].name == aib->mName.C_Str()) {
                    boneIndex = bi; break;
                }
            }
            if (boneIndex < 0) {
                AModelBone nb;
                nb.name = aib->mName.C_Str();
                nb.parent = -1;
                nb.offset = ToXM(aib->mOffsetMatrix);
                nb.local = XMMatrixIdentity();
                nb.global = XMMatrixIdentity();
                boneIndex = (int)bones.size();
                bones.push_back(nb);
            }
            else {
                bones[boneIndex].offset = ToXM(aib->mOffsetMatrix);
            }
            for (uint32_t w = 0; w < aib->mNumWeights; w++) {
                auto& vw = aib->mWeights[w];
                uint32_t vid = sm.vertexOffset + vw.mVertexId;
                for (int s = 0; s < AMODEL_MAX_INFLUENCES; s++) {
                    if (vertices[vid].boneWeights[s] == 0.0f) {
                        vertices[vid].boneIndices[s] = boneIndex;
                        vertices[vid].boneWeights[s] = (float)vw.mWeight;
                        break;
                    }
                }
            }
        }
        for (uint32_t v = 0; v < mesh->mNumVertices; v++) {
            float sum = 0;
            for (int i = 0; i < AMODEL_MAX_INFLUENCES; i++) sum += vertices[sm.vertexOffset + v].boneWeights[i];
            if (sum > 0) {
                float inv = 1.f / sum;
                for (int i = 0; i < AMODEL_MAX_INFLUENCES; i++)
                    vertices[sm.vertexOffset + v].boneWeights[i] *= inv;
            }
            else {
                vertices[sm.vertexOffset + v].boneWeights[0] = 1.0f;
            }
        }
    }

    for (uint32_t f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;
        indices.push_back(sm.vertexOffset + face.mIndices[0]);
        indices.push_back(sm.vertexOffset + face.mIndices[1]);
        indices.push_back(sm.vertexOffset + face.mIndices[2]);
        sm.indexCount += 3;
    }
    submeshes.push_back(sm);
}

bool ModelRuntimeResource::BuildGPU(ID3D11Device* dev) {
    if (!dev) return false;
    if (vertices.empty() || indices.empty()) return false;

    D3D11_BUFFER_DESC bd{};
    D3D11_SUBRESOURCE_DATA init{};

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = (UINT)(sizeof(AModelVertex) * vertices.size());
    init.pSysMem = vertices.data();
    if (FAILED(dev->CreateBuffer(&bd, &init, vb.GetAddressOf()))) return false;

    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = (UINT)(sizeof(uint32_t) * indices.size());
    init.pSysMem = indices.data();
    if (FAILED(dev->CreateBuffer(&bd, &init, ib.GetAddressOf()))) return false;

    return true;
}

void ModelRuntimeResource::ReleaseGPU() {
    vb.Reset();
    ib.Reset();
}

int ModelRuntimeResource::FindBoneIndex(const std::string& name) const {
    for (int i = 0; i < (int)bones.size(); i++)
        if (bones[i].name == name) return i;
    return -1;
}