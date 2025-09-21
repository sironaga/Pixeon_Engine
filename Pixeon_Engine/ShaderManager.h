#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include <d3d11.h>

class ShaderManager {
public:
    static ShaderManager* GetInstance();
    static void DestroyInstance();

    void Initialize(ID3D11Device* device);
    void Finalize();

    // HLSL�e���v���[�g�쐬
    bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type);

    // HLSL�X�V���R���p�C��
    void UpdateAndCompileShaders();

    // �V�F�[�_�[�擾
    ID3D11VertexShader* GetVertexShader(const std::string& name);
    ID3D11PixelShader* GetPixelShader(const std::string& name);

    // �o�b�t�@����
    bool CreateConstantBuffer(const std::string& shaderName, UINT bufferSize);

    // �o�b�t�@�������݁iWriteBuffer�����j
    bool WriteBuffer(const std::string& shaderName, UINT slot, void* pData, UINT dataSize);

    // �o�b�t�@�擾
    ID3D11Buffer* GetConstantBuffer(const std::string& shaderName, UINT slot);


    std::vector<std::string> GetShaderList(const std::string& type) const;
private:
    ShaderManager() = default;
    ~ShaderManager() {};

    bool CompileHLSL(const std::string& hlslPath, const std::string& entry, const std::string& target, const std::string& csoPath);
    bool LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name);

    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::string, ID3D11VertexShader*> m_vsShaders;
    std::unordered_map<std::string, ID3D11PixelShader*> m_psShaders;

    // �V�F�[�_�[���ƂɃo�b�t�@���Ǘ�
    std::unordered_map<std::string, std::vector<ID3D11Buffer*>> m_constantBuffers;

    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

    static ShaderManager* instance;
};