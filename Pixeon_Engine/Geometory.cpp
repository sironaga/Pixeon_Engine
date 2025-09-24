#include "Geometory.h"
#include "ShaderManager.h"
#include "System.h"
#include "IMGUI/imgui.h"
#include "Scene.h"
#include "CameraComponent.h"

Geometory::Geometory() {}
Geometory::~Geometory() {
    if (m_vertexBuffer) m_vertexBuffer->Release();
}

void Geometory::Init(Object* Prt) {
    _Parent = Prt;
    _ComponentName = "Geometory";
    _Type = ComponentManager::COMPONENT_TYPE::GEOMETORY;
}

void Geometory::DrawInspector()
{
    std::string label = EditrGUI::GetInstance()->ShiftJISToUTF8("Geometory");
    std::string Ptr = std::to_string((uintptr_t)this);
    label += "###" + Ptr;

    if (ImGui::CollapsingHeader(EditrGUI::GetInstance()->ShiftJISToUTF8(label).c_str())) {
        if (ImGui::BeginTable("GeometoryTable", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV)) {

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Size");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##GridSize", &m_gridSize, 0.1f, 0.1f, 100.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Count");
            ImGui::TableSetColumnIndex(1); ImGui::DragInt("##GridCount", &m_gridCount, 1, 1, 100);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Grid Color");
            ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4("##GridColor", reinterpret_cast<float*>(&m_gridColor));

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Center");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat3("##CubeCenter", reinterpret_cast<float*>(&m_cubeCenter), 0.1f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Size");
            ImGui::TableSetColumnIndex(1); ImGui::DragFloat("##CubeSize", &m_cubeSize, 0.1f, 0.1f, 100.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Cube Color");
            ImGui::TableSetColumnIndex(1); ImGui::ColorEdit4("##CubeColor", reinterpret_cast<float*>(&m_cubeColor));

            // シェーダー選択
            auto vsList = ShaderManager::GetInstance()->GetShaderList("VS");
            auto psList = ShaderManager::GetInstance()->GetShaderList("PS");
            static int vs_selected = 0;
            static int ps_selected = 0;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Vertex Shader");
            ImGui::TableSetColumnIndex(1);
            if (!vsList.empty()) {
                std::vector<const char*> vsNames;
                for (const auto& name : vsList) vsNames.push_back(name.c_str());
                ImGui::Combo("##VertexShader", &vs_selected, vsNames.data(), (int)vsNames.size());
                m_selectedVS = vsList[vs_selected];
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("Pixel Shader");
            ImGui::TableSetColumnIndex(1);
            if (!psList.empty()) {
                std::vector<const char*> psNames;
                for (const auto& name : psList) psNames.push_back(name.c_str());
                ImGui::Combo("##PixelShader", &ps_selected, psNames.data(), (int)psNames.size());
                m_selectedPS = psList[ps_selected];
            }

            // 設定反映ボタン
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text(EditrGUI::GetInstance()->ShiftJISToUTF8("設定を反映").c_str());
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Button("Apply Settings")) {
                UpdateGridVertices();
                UpdateCubeVertices();
                UpdateVertexBuffer();
                m_vsShader = ShaderManager::GetInstance()->GetVertexShader(m_selectedVS);
                m_psShader = ShaderManager::GetInstance()->GetPixelShader(m_selectedPS);
            }

            ImGui::EndTable();
        }
    }
}

void Geometory::UpdateGridVertices()
{
    m_lineVertices.clear();
    float half = m_gridSize * m_gridCount * 0.5f;
    // X軸方向（Zが変化）
    for (int i = -m_gridCount; i <= m_gridCount; ++i) {
        float x = i * m_gridSize;
        m_lineVertices.push_back({ {x, 0.0f, -half}, m_gridColor });
        m_lineVertices.push_back({ {x, 0.0f,  half}, m_gridColor });
    }
    // Z軸方向（Xが変化）
    for (int i = -m_gridCount; i <= m_gridCount; ++i) {
        float z = i * m_gridSize;
        m_lineVertices.push_back({ {-half, 0.0f, z}, m_gridColor });
        m_lineVertices.push_back({ { half, 0.0f, z}, m_gridColor });
    }
}

void Geometory::UpdateCubeVertices()
{
    float half = m_cubeSize * 0.5f;
    DirectX::XMFLOAT3 v[8] = {
        {m_cubeCenter.x - half, m_cubeCenter.y - half, m_cubeCenter.z - half},
        {m_cubeCenter.x + half, m_cubeCenter.y - half, m_cubeCenter.z - half},
        {m_cubeCenter.x + half, m_cubeCenter.y - half, m_cubeCenter.z + half},
        {m_cubeCenter.x - half, m_cubeCenter.y - half, m_cubeCenter.z + half},
        {m_cubeCenter.x - half, m_cubeCenter.y + half, m_cubeCenter.z - half},
        {m_cubeCenter.x + half, m_cubeCenter.y + half, m_cubeCenter.z - half},
        {m_cubeCenter.x + half, m_cubeCenter.y + half, m_cubeCenter.z + half},
        {m_cubeCenter.x - half, m_cubeCenter.y + half, m_cubeCenter.z + half},
    };
    int edge[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; ++i) {
        m_lineVertices.push_back({ v[edge[i][0]], m_cubeColor });
        m_lineVertices.push_back({ v[edge[i][1]], m_cubeColor });
    }
}

void Geometory::UpdateVertexBuffer()
{
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_lineVertices.empty()) return;
    auto* device = DirectX11::GetInstance()->GetDevice();
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = UINT(sizeof(LineVertex) * m_lineVertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = m_lineVertices.data();
    HRESULT hr = device->CreateBuffer(&bd, &srd, &m_vertexBuffer);
    if (FAILED(hr)) m_vertexBuffer = nullptr;
}

void Geometory::Draw()
{
    auto* context = DirectX11::GetInstance()->GetContext();
    if (!context || !m_vertexBuffer) return;
    if (m_vsShader) context->VSSetShader(m_vsShader, nullptr, 0);
    if (m_psShader) context->PSSetShader(m_psShader, nullptr, 0);

    CameraComponent* camera = nullptr;
    if (_Parent && _Parent->GetParentScene())
        camera = _Parent->GetParentScene()->GetMainCamera();
    if (!camera) return;

    // 転置ありで行列取得
    DirectX::XMFLOAT4X4 view = camera->GetViewMatrix(true);
    DirectX::XMFLOAT4X4 proj = camera->GetProjectionMatrix(true);

    struct CameraCB {
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
    } cb;
    cb.view = view;
    cb.proj = proj;

    if (!m_cameraBuffer) {
        D3D11_BUFFER_DESC cbd{};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(CameraCB);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = &cb;
        DirectX11::GetInstance()->GetDevice()->CreateBuffer(&cbd, &srd, &m_cameraBuffer);
    }
    else {
        context->UpdateSubresource(m_cameraBuffer, 0, nullptr, &cb, 0, 0);
    }
    context->VSSetConstantBuffers(0, 1, &m_cameraBuffer);

    UINT stride = sizeof(LineVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    context->Draw((UINT)m_lineVertices.size(), 0);
}