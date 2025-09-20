#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <Windows.h>
#include <d3d11.h>

class ShaderManager {
public:
    static ShaderManager& GetInstance();
	static void DestroyInstance();

    void Initialize(ID3D11Device* device);
    void Finalize();

    // HLSL新規作成
    bool CreateHLSLTemplate(const std::string& shaderName, const std::string& type); // type: "VS" or "PS"

    // HLSL更新検知 & コンパイル
    void UpdateAndCompileShaders();

    // シェーダー選択リスト取得 (VS/PS)
    std::vector<std::string> GetShaderList(const std::string& type) const;

    // シェーダー取得
    ID3D11VertexShader* GetVertexShader(const std::string& name);
    ID3D11PixelShader* GetPixelShader(const std::string& name);

private:
    ShaderManager() = default;
    ~ShaderManager() {};

    bool CompileHLSL(const std::string& hlslPath, const std::string& entry, const std::string& target, const std::string& csoPath);
    bool LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name);

    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::string, ID3D11VertexShader*> m_vsShaders;
    std::unordered_map<std::string, ID3D11PixelShader*> m_psShaders;

    // HLSL更新時刻記録
    std::unordered_map<std::string, FILETIME> m_hlslUpdateTimes;

	static ShaderManager* instance;
};
