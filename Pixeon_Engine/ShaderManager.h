#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include <d3d11.h>



enum class ShaderStage { VS, PS };

class ShaderManager {
public:
    static ShaderManager* GetInstance();
    static void DestroyInstance();

    void Initialize(ID3D11Device* device);
    void Finalize();

    // HLSL�e���v���[�g�쐬�i�����j
    bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type);

    // HLSL�X�V���R���p�C���i�����j
    void UpdateAndCompileShaders();

    // �V�F�[�_�[�擾�i�����j
    ID3D11VertexShader* GetVertexShader(const std::string& name);
    ID3D11PixelShader* GetPixelShader(const std::string& name);

    // �����̌Œ�L�[��CB�i����݊��p�j
    bool CreateConstantBuffer(const std::string& key, UINT bufferSize);
    bool WriteBuffer(const std::string& key, UINT slot, void* pData, UINT dataSize);
    ID3D11Buffer* GetConstantBuffer(const std::string& key, UINT slot);

    // �V�F�[�_�[�ꗗ�i�����j
    std::vector<std::string> GetShaderList(const std::string& type) const;

    // ���̓��C�A�E�g�p��VS�o�C�g�R�[�h�擾
    bool GetVSBytecode(const std::string& name, const void** ppData, size_t* pSize) const;

    // ��������ǉ�: ����/��CB�Ǘ�
    struct VariableDesc {
        std::string name;
        UINT        offset = 0;
        UINT        size = 0;
    };
    struct CBufferRuntime {
        std::string                     name;
        UINT                            bindPoint = 0;  // register(bX)
        UINT                            size = 0;  // 16�̔{��
        ID3D11Buffer* gpuBuffer = nullptr;
        std::vector<uint8_t>            cpuData;
        bool                            dirty = false;
        std::unordered_map<std::string, VariableDesc> varsByName;
    };
    struct ShaderReflectionData {
        std::vector<CBufferRuntime> cbuffers; // �������݂�����
        // ���O -> index �̌����x��
        std::unordered_map<std::string, size_t> cbufIndexByName;
    };

    // ���ˏ��擾
    const ShaderReflectionData* GetReflection(ShaderStage stage, const std::string& shaderName) const;

    // �ϐ����ŃZ�b�g�i���݂����ꍇ�̂ݏ������ށj
    bool SetCBufferVariable(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const std::string& varName,
        const void* data, UINT size);

    // cbuffer�S�̂��������݁i�T�C�Y��v���ɗL���j
    bool SetCBufferRaw(ShaderStage stage,
        const std::string& shaderName,
        const std::string& cbName,
        const void* data, UINT size);

    // �ύX�̓�����cbuffer�����ׂ�UpdateSubresource���A�K�؂�BindPoint�Ƀo�C���h
    bool CommitAndBind(ShaderStage stage, const std::string& shaderName);

private:
    ShaderManager() = default;
    ~ShaderManager() {};

    bool CompileHLSL(const std::string& hlslPath, const std::string& entry, const std::string& target, const std::string& csoPath);
    bool LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name);

    // ���˂̎���
    bool ReflectShader(ShaderStage stage, const std::string& shaderName, const void* bytecode, size_t size);

    ID3D11Device* m_device = nullptr;

    std::unordered_map<std::string, ID3D11VertexShader*> m_vsShaders;
    std::unordered_map<std::string, ID3D11PixelShader*>  m_psShaders;

    // �萔�o�b�t�@�i�]���̌Œ�L�[�Łj
    std::unordered_map<std::string, std::vector<ID3D11Buffer*>> m_constantBuffers;

    // VS/PS�o�C�g�R�[�h�ێ��i���̓��C�A�E�g�E���˂Ɏg�p�j
    std::unordered_map<std::string, std::vector<char>> m_vsBytecodes;
    std::unordered_map<std::string, std::vector<char>> m_psBytecodes;

    // ���˃f�[�^
    std::unordered_map<std::string, ShaderReflectionData> m_vsRef;
    std::unordered_map<std::string, ShaderReflectionData> m_psRef;

    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

    static ShaderManager* instance;
};