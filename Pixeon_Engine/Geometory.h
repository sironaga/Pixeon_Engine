#pragma once
#include "Component.h"
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include <vector>

class Geometory : public Component
{
public:
    Geometory();
    ~Geometory();

    void Init(Object* Prt) override;
    void DrawInspector() override;
    void Draw() override;

    // パラメータ
    float m_gridSize = 1.0f;
    int   m_gridCount = 10;
    DirectX::XMFLOAT4 m_gridColor = { 0.3f, 0.8f, 0.3f, 1.0f };
    DirectX::XMFLOAT3 m_cubeCenter = { 0, 0.5f, 0 };
    float m_cubeSize = 1.0f;
    DirectX::XMFLOAT4 m_cubeColor = { 1, 1, 0, 1 };

    // シェーダー選択
    std::string m_selectedVS;
    std::string m_selectedPS;
    ID3D11VertexShader* m_vsShader = nullptr;
    ID3D11PixelShader* m_psShader = nullptr;

private:
    struct LineVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };
    std::vector<LineVertex> m_lineVertices;
    ID3D11Buffer* m_vertexBuffer = nullptr;

    ID3D11Buffer* m_cameraBuffer = nullptr;
    void UpdateGridVertices();
    void UpdateCubeVertices();
    void UpdateVertexBuffer();
};