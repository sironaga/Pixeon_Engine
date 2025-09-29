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
    EnsureWhiteTexture();
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

std::string ModelRenderComponent::ResolveTexturePath(const std::string& modelLogical, const std::string& rawPath)
{
    if (rawPath.empty()) return {};

    // 埋め込みテクスチャ (例: "*0") は現バージョン非対応
    if (rawPath[0] == '*') {
        OutputDebugStringA(("[ModelManager] Embedded texture unsupported: " + rawPath + "\n").c_str());
        return {};
    }

    std::string norm = NormalizeRelativePath(rawPath);

    // 絶対パスならファイル名のみ
    {
        std::error_code ec;
        std::filesystem::path p(norm);
        if (p.is_absolute()) {
            norm = p.filename().generic_string();
        }
    }

    // モデルのディレクトリ
    std::string modelDir;
    if (auto pos = modelLogical.find_last_of("/\\"); pos != std::string::npos) {
        modelDir = modelLogical.substr(0, pos + 1); // 末尾に '/'
        for (auto& c : modelDir) if (c == '\\') c = '/';
    }

    // raw のファイル名
    std::string filename = norm;
    if (auto pos = norm.find_last_of('/'); pos != std::string::npos) {
        filename = norm.substr(pos + 1);
    }

    // 候補リスト
    std::vector<std::string> candidates;

    // もし raw がサブフォルダ付き相対ならそのまま最優先
    if (norm.find('/') != std::string::npos) {
        candidates.push_back(norm);
    }
    // 同階層
    candidates.push_back(modelDir + norm);
    // 同階層 + filename (raw がフォルダ付きで存在しない場合への保険)
    candidates.push_back(modelDir + filename);
    // 同階層 Textures/
    candidates.push_back(modelDir + "Textures/" + filename);
    // 共有 Textures/
    candidates.push_back(std::string("Textures/") + filename);
    // ルート直下
    candidates.push_back(filename);

    // 重複削除
    {
        std::vector<std::string> unique;
        unique.reserve(candidates.size());
        std::unordered_set<std::string> seen;
        for (auto& c : candidates) {
            auto n = NormalizeRelativePath(c);
            if (seen.insert(n).second) unique.push_back(n);
        }
        candidates.swap(unique);
    }

    for (auto& c : candidates) {
        if (AssetManager::Instance()->Exists(c)) {
            return c; // 見つかった
        }
    }

    // 見つからず
    OutputDebugStringA(("[ModelManager] Texture not found for material raw=" + rawPath + " (model=" + modelLogical + ")\n").c_str());
    return {}; // 失敗時は空を返し、GUI で上書き可
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

void ModelRenderComponent::ShowTextureSelectPopup(int materialIndex){
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

void ModelRenderComponent::Draw()
{
    if (!m_ready || !m_model) return;

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
        ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get(); // デフォルト白
        if (i < m_materials.size()) {
            auto& mat = m_materials[i];
            if (!mat.tex && !mat.texName.empty()) {
                mat.tex = TextureManager::Instance()->LoadOrGet(mat.texName);
            }
            if (mat.tex && mat.tex->srv) srv = mat.tex->srv.Get();
        }
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);
    }

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    ctx->PSSetShaderResources(0, 1, nullSRV);
}

void ModelRenderComponent::DrawInspector()
{
    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };

    if (ImGui::CollapsingHeader("ModelRenderComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%s %s", SJ("モデル:").c_str(), m_modelPath.c_str());
        static char pathBuf[256];
        std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
        if (ImGui::InputText("Path", pathBuf, sizeof(pathBuf))) {}
        ImGui::SameLine();
        if (ImGui::Button(SJ("手動ロード").c_str())) { SetModel(pathBuf); }
        ImGui::SameLine();
        if (ImGui::Button(SJ("モデル選択...").c_str())) { ImGui::OpenPopup("ModelSelectPopup"); }
        ShowModelSelectPopup();

        // シェーダー設定（既存）... 省略 (前回実装を保持)

        ImGui::ColorEdit4("Color", (float*)&m_color);

        ImGui::Separator();
        ImGui::Text("%s %zu", SJ("マテリアル数:").c_str(), m_materials.size());
        for (size_t i = 0; i < m_materials.size(); ++i) {
            ImGui::PushID((int)i);
            ImGui::Text("Mat %zu", i);
            ImGui::SameLine();
            std::string showPath = m_materials[i].texName.empty() ? SJ("(なし)") : m_materials[i].texName;
            ImGui::Text("%s%s", SJ("tex=").c_str(), showPath.c_str());
            ImGui::SameLine();
            if (ImGui::Button(SJ("テクスチャ選択...").c_str())) {
                m_texPopupMatIndex = (int)i;
                m_openTexPopup = true;
                ImGui::OpenPopup("TextureSelectPopup");
            }
            ImGui::PopID();
        }
        if (m_openTexPopup)
            ShowTextureSelectPopup(m_texPopupMatIndex);
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