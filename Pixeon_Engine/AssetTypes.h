// �A�Z�b�g�̎�ނ��`����w�b�_�[�t�@�C��
// 2025/09/29 By Akino
#pragma once
#include <string>
#include <memory>
#include <vector>
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>

// �e�N�X�`�����\�[�X
struct TextureResource {
	std::string name; // �e�N�X�`����
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv; // �V�F�[�_�[���\�[�X�r���[
	uint32_t width		= 0; // �e�N�X�`���̕�
	uint32_t height		= 0; // �e�N�X�`���̍���
	size_t	gpuBytes	= 0; // GPU�������g�p��
};

// SubMesh���
struct SubMesh {
	uint32_t indexOffset	= 0;		// �C���f�b�N�X�I�t�Z�b�g
	uint32_t indexCount		= 0;		// �C���f�b�N�X��
	uint32_t materialIndex	= 0;		// �}�e���A���C���f�b�N�X
	bool skinned			= false;	// �X�L�j���O�L��
};

// �}�e���A�����ʃf�[�^
struct MaterialShared {
	std::string baseColorTex;				// �e�N�X�`����
	DirectX::XMFLOAT4 baseColor{ 1,1,1,1 }; // �x�[�X�J���[
	float metallic	= 0.0f;					// ���^���b�N
	float roughness = 0.8f;					// ���t�l�X
};

// �{�[�����
struct Bone {
	std::string name;						// �{�[����
	int parentIndex = -1;					// �e�{�[���C���f�b�N�X�i-1�Ȃ�e�Ȃ��j
	DirectX::XMMATRIX offset;
};

// �A�j���[�V�����`�����l��
struct AnimationChannel {
	// ������
};

// �A�j���[�V�����N���b�v
struct AnimationClip {
    std::string name;
    double duration = 0;
    double tps = 25.0;
    std::vector<AnimationChannel> channels;
};

// ���f�����ʃf�[�^
struct ModelSharedResource {
    std::string source;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vb;
    Microsoft::WRL::ComPtr<ID3D11Buffer> ib;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
    std::vector<SubMesh> submeshes;
    std::vector<MaterialShared> materials;
    std::vector<Bone> bones;
    std::vector<AnimationClip> clips;
    bool hasSkin = false;
    size_t gpuBytes = 0;
};

// �T�E���h���\�[�X
struct SoundResource {
    std::string name;
    std::vector<uint8_t> pcmData;
    int channels = 0;
    int sampleRate = 0;
    bool streaming = false;
};

// ���f�����_�t�H�[�}�b�g
struct ModelVertex {
    float position[3];
    float normal[3];
    float tangent[4];
    float uv[2];
    uint32_t boneIndices[4];
    float boneWeights[4];
};