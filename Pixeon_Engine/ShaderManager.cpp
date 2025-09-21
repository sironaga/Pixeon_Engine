#include "ShaderManager.h"
#include "SettingManager.h"
#include <fstream>
#include <d3dcompiler.h>

ShaderManager* ShaderManager::instance = nullptr;

ShaderManager* ShaderManager::GetInstance() {
    if (instance == nullptr) {
        instance = new ShaderManager();
    }
	return instance;
}

void ShaderManager::DestroyInstance() {
    if (instance) {
		instance->Finalize();
        delete instance;
        instance = nullptr;
    }
}

void ShaderManager::Initialize(ID3D11Device* device) {
    m_device = device;
    UpdateAndCompileShaders();
}

void ShaderManager::Finalize() {
    for (auto& kv : m_vsShaders) if (kv.second) kv.second->Release();
    for (auto& kv : m_psShaders) if (kv.second) kv.second->Release();
    m_vsShaders.clear();
    m_psShaders.clear();
}

bool ShaderManager::CreateHLSLTemplate(const std::string& shaderName, const std::string& type) {
    std::string path = SettingManager::GetInstance()->GetShaderFilePath() + shaderName + ".hlsl";
    std::ofstream ofs(path);
    if (!ofs) return false;
    if (type == "VS") {
        ofs << "struct VS_INPUT { float3 pos : POSITION; };\n"
            "struct VS_OUTPUT { float4 pos : SV_POSITION; };\n"
            "VS_OUTPUT main(VS_INPUT input) {\n"
            "    VS_OUTPUT output;\n"
            "    output.pos = float4(input.pos, 1.0f);\n"
            "    return output;\n"
            "}\n";
    }
    else if (type == "PS") {
        ofs << "float4 main(float4 pos : SV_POSITION) : SV_Target {\n"
            "    return float4(1,1,1,1);\n"
            "}\n";
    }
    return true;
}

bool ShaderManager::CompileHLSL(const std::string& hlslPath, const std::string& entry, const std::string& target, const std::string& csoPath) {
    ID3DBlob* code = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompileFromFile(
        std::wstring(hlslPath.begin(), hlslPath.end()).c_str(),
        nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entry.c_str(), target.c_str(),
        0, 0, &code, &error);
    if (FAILED(hr)) {
        if (error) {
            MessageBoxA(nullptr, (char*)error->GetBufferPointer(), "HLSL Compile Error", MB_ICONERROR);
            error->Release();
        }
        if (code) code->Release();
        return false;
    }
    std::ofstream ofs(csoPath, std::ios::binary);
    ofs.write((char*)code->GetBufferPointer(), code->GetBufferSize());
    code->Release();
    return true;
}

void ShaderManager::UpdateAndCompileShaders() {
	std::string hlslDir = SettingManager::GetInstance()->GetShaderFilePath();
	std::string csoDir = SettingManager::GetInstance()->GetCSOFilePath();

    for (const auto& entry : std::filesystem::directory_iterator(hlslDir)) {
        std::string hlslPath = entry.path().string();
        std::string shaderName = entry.path().stem().string();

        WIN32_FILE_ATTRIBUTE_DATA fileInfo;
        GetFileAttributesExA(hlslPath.c_str(), GetFileExInfoStandard, &fileInfo);
        auto it = m_hlslUpdateTimes.find(hlslPath);
        if (it == m_hlslUpdateTimes.end() || CompareFileTime(&it->second, &fileInfo.ftLastWriteTime) < 0) {
            // ファイル名に"_VS"または"_PS"が含まれている場合で判別
            if (shaderName.find("VS_") != std::string::npos) {
                std::string target = "vs_5_0";
                std::string entryFunc = "main";
                std::string csoPath = csoDir + shaderName + ".cso";
                if (CompileHLSL(hlslPath, entryFunc, target, csoPath)) {
                    LoadCSO(csoPath, "VS", shaderName);
                }
            }
            else if (shaderName.find("PS_") != std::string::npos) {
                std::string target = "ps_5_0";
                std::string entryFunc = "main";
                std::string csoPath = csoDir + shaderName + ".cso";
                if (CompileHLSL(hlslPath, entryFunc, target, csoPath)) {
                    LoadCSO(csoPath, "PS", shaderName);
                }
            }
            m_hlslUpdateTimes[hlslPath] = fileInfo.ftLastWriteTime;
        }
    }
}

bool ShaderManager::LoadCSO(const std::string& csoPath, const std::string& type, const std::string& name) {
    std::ifstream ifs(csoPath, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    if (!ifs.read(buffer.data(), size)) return false;

    HRESULT hr = S_OK;
    if (type == "VS") {
        ID3D11VertexShader* vs = nullptr;
        hr = m_device->CreateVertexShader(buffer.data(), size, nullptr, &vs);
        if (SUCCEEDED(hr)) m_vsShaders[name] = vs;
    }
    else if (type == "PS") {
        ID3D11PixelShader* ps = nullptr;
        hr = m_device->CreatePixelShader(buffer.data(), size, nullptr, &ps);
        if (SUCCEEDED(hr)) m_psShaders[name] = ps;
    }
    return SUCCEEDED(hr);
}

std::vector<std::string> ShaderManager::GetShaderList(const std::string& type) const {
    std::vector<std::string> result;
    if (type == "VS") {
        for (const auto& kv : m_vsShaders) result.push_back(kv.first);
    }
    else if (type == "PS") {
        for (const auto& kv : m_psShaders) result.push_back(kv.first);
    }
    return result;
}

ID3D11VertexShader* ShaderManager::GetVertexShader(const std::string& name) {
    auto it = m_vsShaders.find(name);
    return (it != m_vsShaders.end()) ? it->second : nullptr;
}

ID3D11PixelShader* ShaderManager::GetPixelShader(const std::string& name) {
    auto it = m_psShaders.find(name);
    return (it != m_psShaders.end()) ? it->second : nullptr;
}

bool ShaderManager::CreateConstantBuffer(const std::string& shaderName, UINT bufferSize) {
    if (!m_device) return false;
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = bufferSize;
    desc.Usage = D3D11_USAGE_DEFAULT; // UpdateSubresourceで書き込む場合
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    ID3D11Buffer* buffer = nullptr;
    HRESULT hr = m_device->CreateBuffer(&desc, nullptr, &buffer);
    if (FAILED(hr)) return false;

    m_constantBuffers[shaderName].push_back(buffer);
    return true;
}

bool ShaderManager::WriteBuffer(const std::string& shaderName, UINT slot, void* pData, UINT dataSize) {
    auto it = m_constantBuffers.find(shaderName);
    if (it == m_constantBuffers.end()) return false;
    if (slot >= it->second.size()) return false;

    ID3D11DeviceContext* pContext = nullptr;
    m_device->GetImmediateContext(&pContext);
    if (!pContext) return false;

    // バッファ書き込み
    pContext->UpdateSubresource(it->second[slot], 0, nullptr, pData, 0, 0);
    pContext->Release();
    return true;
}

ID3D11Buffer* ShaderManager::GetConstantBuffer(const std::string& shaderName, UINT slot) {
    auto it = m_constantBuffers.find(shaderName);
    if (it == m_constantBuffers.end()) return nullptr;
    if (slot >= it->second.size()) return nullptr;
    return it->second[slot];
}