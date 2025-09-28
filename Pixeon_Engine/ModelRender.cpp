#define NOMINMAX
#include "ModelRender.h"
#include "System.h"
#include "Scene.h"
#include "AssetManager.h"
#include "CameraComponent.h"
#include "SettingManager.h"
#include "IMGUI/imgui.h"

using namespace DirectX;

Microsoft::WRL::ComPtr<ID3D11VertexShader> ModelRenderComponent::s_vs;
Microsoft::WRL::ComPtr<ID3D11PixelShader>  ModelRenderComponent::s_ps;
Microsoft::WRL::ComPtr<ID3D11InputLayout>  ModelRenderComponent::s_layout;
Microsoft::WRL::ComPtr<ID3D11SamplerState> ModelRenderComponent::s_linearSmp;

void ModelRenderComponent::Init(Object* owner)
{
    _Parent = owner;
    _ComponentName = "ModelRender";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
}

bool ModelRenderComponent::SetModel(const std::string& logicalPath)
{
    m_modelPath = logicalPath;
    m_model = ModelManager::Instance()->LoadOrGet(logicalPath);
    if (!m_model) {
        OutputDebugStringA(("[ModelRenderComponent] Failed load model: " + logicalPath + "\n").c_str());
        m_ready = false;
        return false;
    }
    RefreshMaterialCache();
    if (!EnsureShaders()) return false;
    if (!EnsureInputLayout()) return false;
    if (!EnsureConstantBuffer()) return false;
    m_ready = true;
    return true;
}

void ModelRenderComponent::RefreshMaterialCache()
{
    m_materials.clear();
    if (!m_model) return;
    m_materials.reserve(m_model->materials.size());
    for (auto& m : m_model->materials) {
        MaterialRuntime rt;
        rt.texName = m.baseColorTex;
        if (!m.baseColorTex.empty()) {
            rt.tex = TextureManager::Instance()->LoadOrGet(m.baseColorTex);
        }
        rt.color = m.baseColor;
        m_materials.push_back(rt);
    }
}

void ModelRenderComponent::ShowModelSelectPopup(){
    if (ImGui::BeginPopupModal("ModelSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // 文字列変換ヘルパ
        auto SJ = [](const char* s) -> std::string {
            return EditrGUI::GetInstance()->ShiftJISToUTF8(s);
            };

        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ(部分一致)").c_str(), filter, sizeof(filter));

        // AssetManager からモデル候補取得
        auto list = AssetManager::Instance()->GetCachedAssetNames(true);

        // 登録モデル数表示
        {
            std::string label = SJ("登録モデル数: ") + std::to_string(list.size());
            ImGui::Text("%s", label.c_str());
        }

        ImGui::Separator();
        ImGui::BeginChild("ModelSelectList", ImVec2(420, 320), true);
        static int currentHighlight = -1;

        for (int i = 0; i < (int)list.size(); ++i)
        {
            const std::string& rawName = list[i];

            // フィルタ（RAW名で判定）
            if (filter[0] != 0 && rawName.find(filter) == std::string::npos)
                continue;

            // 表示用 UTF8 変換
            std::string dispName = EditrGUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());

            bool selected = (currentHighlight == i);
            if (ImGui::Selectable(dispName.c_str(), selected))
            {
                currentHighlight = i;
                // ダブルクリック即適用
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (SetModel(rawName)) {
                        m_modelPath = rawName;
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            // 現在使用中モデル
            if (m_modelPath == rawName) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), SJ("[Using]").c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Separator();

        // 適用ボタン
        if (ImGui::Button(SJ("適用").c_str()))
        {
            if (currentHighlight >= 0 && currentHighlight < (int)list.size()) {
                const std::string& sel = list[currentHighlight];
                if (SetModel(sel)) {
                    m_modelPath = sel;
                }
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("キャンセル").c_str())) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("再ロード").c_str())) {
            if (!m_modelPath.empty()) {
                SetModel(m_modelPath);
            }
        }

        ImGui::EndPopup();
    }
}

bool ModelRenderComponent::EnsureShaders()
{
    //if (s_vs && s_ps) return true;

    // ShaderManager を使って自動コンパイル済みのものを取得
    // HLSL ファイル名 VS_ModelBasic.hlsl → シェーダ名は VS_ModelBasic
    auto* sm = ShaderManager::GetInstance();
    auto* vs = sm->GetVertexShader("VS_ModelBasic");
    auto* ps = sm->GetPixelShader("PS_ModelBasic");
    if (!vs || !ps) {
        // まだコンパイル前の場合は UpdateAndCompileShaders() を呼んで再試行
        sm->UpdateAndCompileShaders();
        vs = sm->GetVertexShader("VS_ModelBasic");
        ps = sm->GetPixelShader("PS_ModelBasic");
        if (!vs || !ps) {
            OutputDebugStringA("[ModelRenderComponent] VS/PS not found. Place HLSL files.\n");
            return false;
        }
    }
    s_vs = vs;
    s_ps = ps;

    // サンプラ作成
    if (!s_linearSmp) {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        sd.MinLOD = 0;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        auto dev = DirectX11::GetInstance()->GetDevice();
        dev->CreateSamplerState(&sd, s_linearSmp.GetAddressOf());
    }
    return true;
}

bool ModelRenderComponent::EnsureInputLayout()
{
    if (s_layout) return true;
    const void* bc = nullptr;
    size_t bcSize = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode("VS_ModelBasic", &bc, &bcSize)) {
        OutputDebugStringA("[ModelRenderComponent] VS bytecode not found for input layout.\n");
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC desc[] =
    {
        {"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,      0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    auto dev = DirectX11::GetInstance()->GetDevice();
    HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), bc, bcSize, s_layout.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ModelRenderComponent] CreateInputLayout failed\n");
        return false;
    }
    return true;
}

bool ModelRenderComponent::EnsureConstantBuffer()
{
    if (m_cb) return true;
    auto dev = DirectX11::GetInstance()->GetDevice();
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(CBData);
    bd.Usage = D3D11_USAGE_DEFAULT;
    HRESULT hr = dev->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ModelRenderComponent] Create constant buffer failed\n");
        return false;
    }
    return true;
}

DirectX::XMMATRIX ModelRenderComponent::BuildWorldMatrix() const
{
    Transform t = _Parent->GetTransform();
    XMMATRIX S = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
    XMMATRIX T = XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
    return S * R * T;
}

void ModelRenderComponent::Draw()
{
    if (!m_ready || !m_model) return;

	MessageBox(nullptr, "Debug Break in ModelRenderComponent::Draw", "Debug", MB_OK);

    // シーンとカメラ取得
    Scene* scene = _Parent->GetParentScene();
    if (!scene) return;
    CameraComponent* cam = scene->GetMainCamera();
    if (!cam) return;

    // 以前: cam->GetViewMatrix(); cam->GetProjMatrix(); → 存在しない
    // 修正: CameraComponent の XMMATRIX 版を使用
    DirectX::XMMATRIX view = cam->GetView();
    DirectX::XMMATRIX proj = cam->GetProjection();
    DirectX::XMMATRIX world = BuildWorldMatrix();

    // 定数バッファ更新（HLSL 側は column_major 想定なので Transpose 渡し）
    CBData cbd;
    cbd.World = DirectX::XMMatrixTranspose(world);
    cbd.View = DirectX::XMMatrixTranspose(view);
    cbd.Proj = DirectX::XMMatrixTranspose(proj);
    cbd.BaseColor = m_color;

    auto ctx = DirectX11::GetInstance()->GetContext();
    ctx->UpdateSubresource(m_cb.Get(), 0, nullptr, &cbd, 0, 0);

    UINT stride = sizeof(ModelVertex);
    UINT offset = 0;
    ID3D11Buffer* vb = m_model->vb.Get();
    ID3D11Buffer* ib = m_model->ib.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
    ctx->IASetInputLayout(s_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(s_vs.Get(), nullptr, 0);
    ctx->PSSetShader(s_ps.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_cb.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);
    ID3D11SamplerState* smp = s_linearSmp.Get();
    ctx->PSSetSamplers(0, 1, &smp);

    for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
        const auto& sm = m_model->submeshes[i];
        ID3D11ShaderResourceView* srv = nullptr;
        if (i < m_materials.size()) {
            if (!m_materials[i].tex && !m_materials[i].texName.empty()) {
                m_materials[i].tex = TextureManager::Instance()->LoadOrGet(m_materials[i].texName);
            }
            if (m_materials[i].tex) srv = m_materials[i].tex->srv.Get();
        }
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);
}

void ModelRenderComponent::DrawInspector()
{
    if (ImGui::CollapsingHeader("ModelRenderComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Model: %s", m_modelPath.c_str());
        static char pathBuf[256];
        std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
        if (ImGui::InputText("Path (manual)", pathBuf, sizeof(pathBuf))) {
            // 入力のみ。確定はボタン。
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Manual")) {
            SetModel(pathBuf);
        }

        // モデル選択ポップアップ表示トリガ
        if (ImGui::Button(EditrGUI::GetInstance()->ShiftJISToUTF8("モデル選択...").c_str())) {
            ImGui::OpenPopup("ModelSelectPopup");
        }
        // ポップアップ本体
        ShowModelSelectPopup();

        ImGui::ColorEdit4("Color", (float*)&m_color);
        ImGui::Text("Materials: %zu", m_materials.size());
        for (size_t i = 0; i < m_materials.size(); ++i) {
            ImGui::PushID((int)i);
            ImGui::Text("Mat %zu: tex=%s", i, m_materials[i].texName.c_str());
            ImGui::PopID();
        }
    }
}

void ModelRenderComponent::SaveToFile(std::ostream& out){
    // シンプル: 1行 JSON 風 or Key=Value
    out << m_modelPath << "\n";
    out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << "\n";
}

void ModelRenderComponent::LoadFromFile(std::istream& in){
    std::getline(in, m_modelPath);
    if (m_modelPath.size() && (m_modelPath.back() == '\r')) m_modelPath.pop_back();
    in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (!m_modelPath.empty()) SetModel(m_modelPath);
}