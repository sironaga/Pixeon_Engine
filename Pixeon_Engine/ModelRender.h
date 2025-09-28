#pragma once
#include "Component.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ShaderManager.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class ModelRenderComponent : public Component
{
public:
    ModelRenderComponent() = default;
    ~ModelRenderComponent()= default;

    void Init(Object* owner) override;
    void Draw() override;
    void DrawInspector() override;

    bool SetModel(const std::string& logicalPath);
    const std::string& GetModelPath() const { return m_modelPath; }

    void SetColor(const DirectX::XMFLOAT4& c) { m_color = c; }
    DirectX::XMFLOAT4 GetColor() const { return m_color; }

    // セーブ / ロード（シーン保存用）
    void SaveToFile(std::ostream& out) override;
    void LoadFromFile(std::istream& in) override;

private:
    struct CBData
    {
        DirectX::XMMATRIX World;
        DirectX::XMMATRIX View;
        DirectX::XMMATRIX Proj;
        DirectX::XMFLOAT4 BaseColor;
    };

    struct MaterialRuntime
    {
        std::string                           texName;
        std::shared_ptr<TextureResource>      tex; // null 可
        DirectX::XMFLOAT4                     color;
    };

    bool EnsureShaders();
    bool EnsureInputLayout();
    bool EnsureConstantBuffer();
    void RefreshMaterialCache();
    void ShowModelSelectPopup();

    DirectX::XMMATRIX BuildWorldMatrix() const;

private:
    std::string m_modelPath;
    std::shared_ptr<ModelSharedResource> m_model;
    std::vector<MaterialRuntime> m_materials;

    DirectX::XMFLOAT4 m_color{ 1,1,1,1 };  // 上書きカラー（単純化）

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cb;

    // 共有リソース（最初のインスタンスで作成）
    static Microsoft::WRL::ComPtr<ID3D11VertexShader> s_vs;
    static Microsoft::WRL::ComPtr<ID3D11PixelShader>  s_ps;
    static Microsoft::WRL::ComPtr<ID3D11InputLayout>  s_layout;
    static Microsoft::WRL::ComPtr<ID3D11SamplerState> s_linearSmp;

    bool m_ready = false;
};