#pragma once
#include <d3d11.h>
#include <string>

// �|�X�g�G�t�F�N�g�̒��ۊ��N���X
class PostEffectBase
{
public:
    virtual ~PostEffectBase() {}

    // �������i���\�[�X�m�ۂȂǁj
    virtual void Init(ID3D11Device* device, int width, int height) = 0;
    // ���
    virtual void UnInit() = 0;

    // �K�p�iSRV����RTV�փG�t�F�N�g��������j
    virtual void Apply(
        ID3D11DeviceContext* context,
        ID3D11ShaderResourceView* inputSRV,
        ID3D11RenderTargetView* outputRTV,
        int viewX, int viewY, int viewWidth, int viewHeight
    ) = 0;

    // �f�o�b�OUI�iImGui�Ȃǁj
    virtual void DrawDebugUI() {}

    // �G�t�F�N�g���i���ʗp�j
    virtual std::string GetEffectName() const = 0;
};