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

    // HLSLテンプレート作成
    bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type);

    // HLSL更新＆コンパイル
    void UpdateAndCompileShaders();

    // シェーダー取得
    ID3D11VertexShader* GetVertexShader(const std::string& name);
    ID3D11PixelShader* GetPixelShader(const std::string& name);

    // バッファ生成
    bool CreateConstantBuffer(const std::string& shaderName, UINT bufferSize);

    // バッファ書き込み（WriteBuffer相当）
    bool WriteBuffer(const std::string& shaderName, UINT slot, void* pData, UINT dataSize);

    // バッファ取得
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

    // シェーダーごとにバッファを管理
    std::unordered_map<std::string, std::vector<ID3D11Buffer*>> m_constantBuffers;

    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

    static ShaderManager* instance;
};