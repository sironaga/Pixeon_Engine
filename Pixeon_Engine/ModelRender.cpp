#define NOMINMAX
#include "ModelRender.h"
#include "System.h"
#include "Scene.h"
#include "AssetManager.h"
#include "CameraComponent.h"
#include "SettingManager.h"
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include <filesystem>

using namespace DirectX;

Microsoft::WRL::ComPtr<ID3D11VertexShader> ModelRenderComponent::s_vs;
Microsoft::WRL::ComPtr<ID3D11PixelShader>  ModelRenderComponent::s_ps;
Microsoft::WRL::ComPtr<ID3D11InputLayout>  ModelRenderComponent::s_layout;
Microsoft::WRL::ComPtr<ID3D11SamplerState> ModelRenderComponent::s_linearSmp;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_whiteTexSRV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_magentaTexSRV;

static std::string NormalizeRelativePath(std::string s)
{
    for (auto& c : s) if (c == '\\') c = '/';
    // 先頭 "./" や "/" を除去
    while (!s.empty()) {
        if (s.rfind("./", 0) == 0) { s.erase(0, 2); continue; }
        if (s[0] == '/') { s.erase(0, 1); continue; }
        break;
    }
    // 重複 "//" を単一化
    std::string out; out.reserve(s.size());
    bool prevSlash = false;
    for (char c : s) {
        if (c == '/') {
            if (!prevSlash) out.push_back(c);
            prevSlash = true;
        }
        else {
            prevSlash = false;
            out.push_back(c);
        }
    }
    return out;
}

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
    if (!EnsureShaders(false)) return false;
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

bool ModelRenderComponent::EnsureShaders(bool forceRecreateLayout)
{
    auto* sm = ShaderManager::GetInstance();

    ID3D11VertexShader* vs = sm->GetVertexShader(m_vsName);
    ID3D11PixelShader* ps = sm->GetPixelShader(m_psName);

    if (!vs || !ps) {
        sm->UpdateAndCompileShaders();
        vs = sm->GetVertexShader(m_vsName);
        ps = sm->GetPixelShader(m_psName);
        if (!vs || !ps) {
            std::string msg = "[ModelRenderComponent] シェーダーが見つかりません: " + m_vsName + ", " + m_psName + "\n";
            OutputDebugStringA(msg.c_str());
            return false;
        }
    }

    // 変更検知 (名前が変わった場合に再設定)
    s_vs = vs;
    s_ps = ps;

    if (forceRecreateLayout || !s_layout) {
        RecreateInputLayout();
    }

    // サンプラ
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
    EnsureDebugFallbackTextures();
    return true;
}

void ModelRenderComponent::RecreateInputLayout()
{
    s_layout.Reset();
    const void* bc = nullptr;
    size_t bcSize = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(m_vsName, &bc, &bcSize)) {
        OutputDebugStringA("[ModelRenderComponent] VS bytecode 取得失敗(InputLayout再構築)\n");
        return;
    }
    EnsureInputLayout(bc, bcSize);
}

bool ModelRenderComponent::EnsureInputLayout(const void* vsBytecode, size_t size)
{
    if (s_layout) return true;

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
    HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), vsBytecode, size, s_layout.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ModelRenderComponent] InputLayout 作成失敗\n");
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
        OutputDebugStringA("[ModelRenderComponent] 定数バッファ作成失敗\n");
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

bool ModelRenderComponent::EnsureWhiteTexture()
{
    if (s_whiteTexSRV) return true;
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;

    // 1x1 RGBA (255,255,255,255)
    uint32_t pixel = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1;
    td.Height = 1;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel;
    init.SysMemPitch = 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = dev->CreateTexture2D(&td, &init, tex.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    return true;
}

bool ModelRenderComponent::EnsureDebugFallbackTextures(){
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;
    if (!s_whiteTexSRV) {
        uint32_t pixel = 0xFFFFFFFF;
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ &pixel, 4, 4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
    }
    if (!s_magentaTexSRV) {
        uint32_t pixel = 0xFFFF00FF; // ARGB or RGBA? -> DirectXTex Tex2D raw -> little endian 0xAABBGGRR
        uint8_t pix[4] = { 255, 0, 255, 255 }; // RGBA = Magenta
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ pix, 4, 4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_magentaTexSRV.GetAddressOf());
    }
    return (s_whiteTexSRV && s_magentaTexSRV);
}

void ModelRenderComponent::Draw()
{
    if (!m_ready || !m_model) return;


    static bool s_onceLogged = false;
    if (!s_onceLogged) {
        char buf[256];
        sprintf_s(buf, "[ModelRender] model=%s ready=%d submeshes=%zu mats=%zu vb=%p ib=%p vs=%s ps=%s\n",
            m_modelPath.c_str(), (int)m_ready, m_model ? m_model->submeshes.size() : 0,
            m_materials.size(),
            m_model ? m_model->vb.Get() : nullptr,
            m_model ? m_model->ib.Get() : nullptr,
            m_vsName.c_str(), m_psName.c_str());
        OutputDebugStringA(buf);
        for (size_t i = 0; i < m_materials.size(); ++i) {
            std::string t = m_materials[i].texName.empty() ? "(none)" : m_materials[i].texName;
            std::string l = "[ModelRender] Mat" + std::to_string(i) + " tex=" + t + "\n";
            OutputDebugStringA(l.c_str());
        }
        s_onceLogged = true;
    }

    Scene* scene = _Parent->GetParentScene();
    if (!scene) return;
    CameraComponent* cam = scene->GetMainCamera();
    if (!cam) return;

    XMMATRIX view = cam->GetView();
    XMMATRIX proj = cam->GetProjection();
    XMMATRIX world = BuildWorldMatrix();

    CBData cbd;
    cbd.World = XMMatrixTranspose(world);
    cbd.View = XMMatrixTranspose(view);
    cbd.Proj = XMMatrixTranspose(proj);
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

    EnsureWhiteTexture();

    for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
        const auto& sm = m_model->submeshes[i];
        size_t matIndex = sm.materialIndex;
        ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get(); // 初期は白 (まだロード前)

        if (matIndex < m_materials.size()) {
            auto& mat = m_materials[matIndex];
            if (!mat.tex && !mat.texName.empty()) {
                mat.tex = TextureManager::Instance()->LoadOrGet(mat.texName);
                if (mat.tex && mat.tex->srv) {
                    OutputDebugStringA(("[ModelRender] Texture OK: " + mat.texName + "\n").c_str());
                }
                else {
                    OutputDebugStringA(("[ModelRender] Texture FAIL -> magenta: " + mat.texName + "\n").c_str());
                    srv = s_magentaTexSRV.Get();
                }
            }
            if (mat.tex && mat.tex->srv) {
                srv = mat.tex->srv.Get();
            }
            else if (mat.texName.empty()) {
                // テクスチャ名空: マゼンタで強調
                srv = s_magentaTexSRV.Get();
            }
        }
        else {
            srv = s_magentaTexSRV.Get();
            char logBuf[128];
            sprintf_s(logBuf, "[ModelRender] submesh=%zu materialIndex=%u OUT_OF_RANGE mats=%zu\n",
                i, sm.materialIndex, m_materials.size());
			MessageBox(nullptr, "ModelRenderComponent: Material index out of range!", "Error", MB_OK | MB_ICONERROR);
            OutputDebugStringA(logBuf);
        }

        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }
}


void ModelRenderComponent::SaveToFile(std::ostream& out)
{
    out << m_modelPath << "\n";
    out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << "\n";
    out << m_vsName << "\n" << m_psName << "\n"; // *** シェーダー設定も保存
}

void ModelRenderComponent::LoadFromFile(std::istream& in)
{
    std::getline(in, m_modelPath);
    if (!m_modelPath.empty() && m_modelPath.back() == '\r') m_modelPath.pop_back();
    in >> m_color.x >> m_color.y >> m_color.z >> m_color.w;
    in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    std::getline(in, m_vsName);
    if (!m_vsName.empty() && m_vsName.back() == '\r') m_vsName.pop_back();
    std::getline(in, m_psName);
    if (!m_psName.empty() && m_psName.back() == '\r') m_psName.pop_back();

    if (!m_modelPath.empty()) SetModel(m_modelPath);
}


void ModelRenderComponent::DrawInspector()
{
    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };

    if (!ImGui::CollapsingHeader("ModelRenderComponent", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    // モデルパス
    ImGui::Text("%s %s", SJ("モデル:").c_str(), m_modelPath.c_str());
    static char pathBuf[256];
    std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
    ImGui::InputText("Path", pathBuf, sizeof(pathBuf));
    ImGui::SameLine();
    if (ImGui::Button(SJ("手動ロード").c_str()))
        SetModel(pathBuf);

    ImGui::SameLine();
    if (ImGui::Button(SJ("モデル選択...").c_str()))
        ImGui::OpenPopup("ModelSelectPopup");
    ShowModelSelectPopup();

    // シェーダー設定
    if (ImGui::TreeNode(SJ("シェーダー設定").c_str()))
    {
        auto* sm = ShaderManager::GetInstance();
        static std::vector<std::string> vsList;
        static std::vector<std::string> psList;
        if (vsList.empty()) vsList = sm->GetShaderList("VS");
        if (psList.empty()) psList = sm->GetShaderList("PS");

        if (ImGui::BeginCombo(SJ("頂点シェーダー").c_str(), m_vsName.c_str())) {
            for (auto& n : vsList) {
                bool sel = (n == m_vsName);
                if (ImGui::Selectable(n.c_str(), sel)) { m_vsName = n; EnsureShaders(true); }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo(SJ("ピクセルシェーダー").c_str(), m_psName.c_str())) {
            for (auto& n : psList) {
                bool sel = (n == m_psName);
                if (ImGui::Selectable(n.c_str(), sel)) { m_psName = n; EnsureShaders(false); }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button(SJ("シェーダーリスト再取得").c_str())) {
            vsList = sm->GetShaderList("VS");
            psList = sm->GetShaderList("PS");
        }
        ImGui::TreePop();
    }

    ImGui::ColorEdit4("Color", (float*)&m_color);

    ImGui::Separator();
    ImGui::Text("%s %zu", SJ("マテリアル数:").c_str(), m_materials.size());

    // マテリアルごとの UI
    for (size_t i = 0; i < m_materials.size(); ++i)
    {
        ImGui::PushID((int)i);
        ImGui::Text("Mat %zu", i);
        ImGui::SameLine();
        std::string shown = m_materials[i].texName.empty() ? SJ("(なし)") : m_materials[i].texName;
        ImGui::Text("%s%s", SJ("tex=").c_str(), shown.c_str());
        ImGui::SameLine();
        if (ImGui::Button(SJ("テクスチャ選択...").c_str())) {
            m_texPopupMatIndex = (int)i;
            ImGui::OpenPopup("TextureSelectPopup"); // ★ この PushID スタック内で呼ぶ
        }

        // ★ ポップアップも同じ PushID 内で開始することで ID 不一致を解消
        if (ImGui::BeginPopup("TextureSelectPopup"))
        {
            auto texList = AssetManager::Instance()->GetCachedTextureNames();
            static char texFilter[128] = "";
            ImGui::InputText(SJ("フィルタ").c_str(), texFilter, sizeof(texFilter));

            ImGui::BeginChild("TexListChild", ImVec2(380, 230), true);
            int shownCount = 0;
            for (int ti = 0; ti < (int)texList.size(); ++ti) {
                const std::string& name = texList[ti];
                if (texFilter[0] && name.find(texFilter) == std::string::npos)
                    continue;
                ++shownCount;
                if (ImGui::Selectable(name.c_str(), false)) {
                    if (m_texPopupMatIndex == (int)i) {
                        m_materials[i].texName = name;
                        m_materials[i].tex.reset(); // 再ロード
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            if (texList.empty())
                ImGui::TextDisabled("%s", SJ("(キャッシュにテクスチャがありません)").c_str());
            else if (shownCount == 0)
                ImGui::TextDisabled("%s", SJ("(フィルタに一致する項目なし)").c_str());
            ImGui::EndChild();

            if (ImGui::Button(SJ("閉じる").c_str()))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void ModelRenderComponent::ShowModelSelectPopup()
{
    if (ImGui::BeginPopupModal("ModelSelectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };

        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ(部分一致)").c_str(), filter, sizeof(filter));

        auto list = AssetManager::Instance()->GetCachedAssetNames(true);

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
            if (filter[0] && rawName.find(filter) == std::string::npos) continue;

            std::string dispName = EditrGUI::GetInstance()->ShiftJISToUTF8(rawName.c_str());
            bool selected = (currentHighlight == i);
            if (ImGui::Selectable(dispName.c_str(), selected))
            {
                currentHighlight = i;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (SetModel(rawName)) {
                        m_modelPath = rawName;
                    }
                    ImGui::CloseCurrentPopup();
                }
            }
            if (m_modelPath == rawName) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), SJ("[Using]").c_str());
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        if (ImGui::Button(SJ("適用").c_str()))
        {
            if (currentHighlight >= 0 && currentHighlight < (int)list.size()) {
                const std::string& sel = list[currentHighlight];
                if (SetModel(sel)) m_modelPath = sel;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("キャンセル").c_str())) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("再ロード").c_str())) {
            if (!m_modelPath.empty()) SetModel(m_modelPath);
        }

        ImGui::EndPopup();
    }
}

void ModelRenderComponent::ShowTextureSelectPopup(int materialIndex) {
    if (!m_openTexPopup || materialIndex < 0 || materialIndex >= (int)m_materials.size())
    {
        MessageBox(nullptr, "Invalid state in ShowTextureSelectPopup", "Error", MB_OK);
        return;
    }

    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };

    std::string popupId = "TextureSelectPopup";
    ImGui::SetNextWindowSize(ImVec2(420, 400), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal(popupId.c_str(), &m_openTexPopup, ImGuiWindowFlags_NoCollapse))
    {
        static char filter[128] = "";
        ImGui::InputText(SJ("フィルタ").c_str(), filter, sizeof(filter));

        auto texList = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::Text(SJ("テクスチャ数: %d").c_str(), (int)texList.size());
        ImGui::Separator();

        ImGui::BeginChild("TexList", ImVec2(0, 280), true);
        for (int i = 0; i < (int)texList.size(); ++i) {
            const std::string& name = texList[i];
            if (filter[0] && name.find(filter) == std::string::npos) continue;

            bool select = false;
            if (ImGui::Selectable(name.c_str(), select)) {
                // 選択で即適用
                m_materials[materialIndex].texName = name;
                m_materials[materialIndex].tex.reset(); // 再ロードさせる
                m_openTexPopup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();

        if (ImGui::Button(SJ("閉じる").c_str(), ImVec2(80, 0))) {
            m_openTexPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
