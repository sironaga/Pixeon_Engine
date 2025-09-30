#define NOMINMAX
#include "ModelRender.h"
#include "System.h"
#include "Scene.h"
#include "AssetManager.h"
#include "CameraComponent.h"
#include "SettingManager.h"
#include "EditrGUI.h"
#include "IMGUI/imgui.h"
#include "ErrorLog.h"

using namespace DirectX;

// static (���L) �� sampler �� fallback �e�N�X�`������
Microsoft::WRL::ComPtr<ID3D11SamplerState>       ModelRenderComponent::s_linearSmp;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_whiteTexSRV;
Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ModelRenderComponent::s_magentaTexSRV;

void ModelRenderComponent::Init(Object* owner) {
    _Parent = owner;
    _ComponentName = "ModelRender";
    _Type = ComponentManager::COMPONENT_TYPE::MODEL;
}

bool ModelRenderComponent::SetModel(const std::string& logicalPath) {
    m_modelPath = logicalPath;
    m_model = ModelManager::Instance()->LoadOrGet(logicalPath);
    if (!m_model) {
        ErrorLogger::Instance().LogError("ModelRenderComponent", "Failed load model: " + logicalPath);
        m_ready = false;
        return false;
    }
    RefreshMaterialCache();
    if (!EnsureShaders(true)) return false;
    if (!EnsureConstantBuffer()) return false;
    m_texIssueReported.assign(m_model->submeshes.size(), 0);
    m_ready = true;
    return true;
}

void ModelRenderComponent::RefreshMaterialCache() {
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

bool ModelRenderComponent::EnsureShaders(bool forceRecreateLayout) {
    auto* sm = ShaderManager::GetInstance();

    ID3D11VertexShader* vs = sm->GetVertexShader(m_vsName);
    ID3D11PixelShader* ps = sm->GetPixelShader(m_psName);

    if (!vs || !ps) {
        sm->UpdateAndCompileShaders();
        vs = sm->GetVertexShader(m_vsName);
        ps = sm->GetPixelShader(m_psName);
        if (!vs || !ps) {
            std::string msg = "[ModelRenderComponent] �V�F�[�_�擾���s: " + m_vsName + ", " + m_psName + "\n";
            OutputDebugStringA(msg.c_str());
            return false;
        }
    }

    // �|�C���^�ω����o (�ăR���p�C����Ȃ�)
    bool vsChanged = (m_vs.Get() != vs);
    bool needLayout = forceRecreateLayout || vsChanged || !m_layout;

    m_vs = vs;
    m_ps = ps;

    if (needLayout) {
        RecreateInputLayout();
    }

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

void ModelRenderComponent::RecreateInputLayout() {
    m_layout.Reset();
    const void* bc = nullptr;
    size_t bcSize = 0;
    if (!ShaderManager::GetInstance()->GetVSBytecode(m_vsName, &bc, &bcSize)) {
        OutputDebugStringA("[ModelRenderComponent] VS bytecode �擾���s(InputLayout)\n");
        return;
    }
    EnsureInputLayout(bc, bcSize);
}

bool ModelRenderComponent::EnsureInputLayout(const void* vsBytecode, size_t size) {
    if (m_layout) return true;
    D3D11_INPUT_ELEMENT_DESC desc[] = {
        { "POSITION",0, DXGI_FORMAT_R32G32B32_FLOAT,    0,(UINT)offsetof(ModelVertex,position),    D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "NORMAL",  0, DXGI_FORMAT_R32G32B32_FLOAT,    0,(UINT)offsetof(ModelVertex,normal),      D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,(UINT)offsetof(ModelVertex,tangent),     D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,       0,(UINT)offsetof(ModelVertex,uv),          D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "BLENDINDICES",0, DXGI_FORMAT_R32G32B32A32_UINT, 0,(UINT)offsetof(ModelVertex,boneIndices), D3D11_INPUT_PER_VERTEX_DATA,0 },
        { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,0,(UINT)offsetof(ModelVertex,boneWeights), D3D11_INPUT_PER_VERTEX_DATA,0 },
    };
    auto dev = DirectX11::GetInstance()->GetDevice();
    HRESULT hr = dev->CreateInputLayout(desc, _countof(desc), vsBytecode, size, m_layout.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ModelRenderComponent] InputLayout �쐬���s\n");
        return false;
    }
    return true;
}

bool ModelRenderComponent::EnsureConstantBuffer() {
    if (m_cb) return true;
    auto dev = DirectX11::GetInstance()->GetDevice();
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(CBData);
    bd.Usage = D3D11_USAGE_DEFAULT;
    HRESULT hr = dev->CreateBuffer(&bd, nullptr, m_cb.GetAddressOf());
    if (FAILED(hr)) {
        OutputDebugStringA("[ModelRenderComponent] �萔�o�b�t�@�쐬���s\n");
        return false;
    }
    return true;
}

DirectX::XMMATRIX ModelRenderComponent::BuildWorldMatrix() const {
    Transform t = _Parent->GetTransform();
    XMMATRIX S = XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z);
    XMMATRIX T = XMMatrixTranslation(t.position.x, t.position.y, t.position.z);
    return S * R * T;
}

bool ModelRenderComponent::EnsureWhiteTexture() {
    if (s_whiteTexSRV) return true;
    auto dev = DirectX11::GetInstance()->GetDevice();
    if (!dev) return false;
    uint32_t pixel = 0xFFFFFFFF;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = 1; td.Height = 1;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = &pixel; init.SysMemPitch = 4;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (FAILED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf()))) return false;
    if (FAILED(dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf()))) return false;
    return true;
}

bool ModelRenderComponent::EnsureDebugFallbackTextures() {
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
        D3D11_SUBRESOURCE_DATA init{ &pixel,4,4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_whiteTexSRV.GetAddressOf());
    }
    if (!s_magentaTexSRV) {
        uint8_t pix[4] = { 255,255,255,255 };
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = 1;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{ pix,4,4 };
        Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
        if (SUCCEEDED(dev->CreateTexture2D(&td, &init, tex.GetAddressOf())))
            dev->CreateShaderResourceView(tex.Get(), nullptr, s_magentaTexSRV.GetAddressOf());
    }
    return (s_whiteTexSRV && s_magentaTexSRV);
}

void ModelRenderComponent::DiagnoseAndReportTextureIssue(size_t submeshIdx,
    const SubMesh& sm, const MaterialRuntime* mat,
    ID3D11ShaderResourceView* chosenSRV, bool usedMagentaFallback, bool usedWhiteFallback)
{
    if (submeshIdx >= m_texIssueReported.size()) return;
    if (m_texIssueReported[submeshIdx]) return;

    TextureIssue issue = TextureIssue::None;
    std::string detail;

    if (sm.materialIndex >= m_materials.size()) {
        issue = TextureIssue::MaterialIndexOutOfRange;
        detail = "submesh.materialIndex=" + std::to_string(sm.materialIndex);
    }
    else {
        const MaterialRuntime* mrt = mat;
        if (!mrt) {
            issue = TextureIssue::TextureSRVNull;
            detail = "MaterialRuntime null";
        }
        else {
            if (mrt->texName.empty()) {
                issue = TextureIssue::MaterialNoPath;
                detail = "Material has no texture path";
            }
            else if (!mrt->tex) {
                issue = TextureIssue::TextureLoadFailed;
                detail = "LoadOrGet null " + mrt->texName;
            }
            else if (mrt->tex && !mrt->tex->srv) {
                issue = TextureIssue::TextureSRVNull;
                detail = "SRV null " + mrt->texName;
            }
            if (issue == TextureIssue::None) {
                if (!sm.hasUV) { issue = TextureIssue::NoUVChannel; detail = "No UV"; }
                else if (sm.uvAllZero) { issue = TextureIssue::UVAllZero; detail = "All UV zero"; }
            }
        }
    }
    if (issue == TextureIssue::None && !s_linearSmp) {
        issue = TextureIssue::SamplerMissing; detail = "Sampler missing";
    }
    if (issue == TextureIssue::None) {
        if (usedMagentaFallback) { issue = TextureIssue::StillFallbackMagenta; detail = "Magenta fallback"; }
        else if (usedWhiteFallback) { issue = TextureIssue::StillFallbackWhite; detail = "White fallback"; }
    }

    if (issue != TextureIssue::None) {
        m_texIssueReported[submeshIdx] = 1;
        static const char* issueNames[] = {
            "None","MaterialIndexOutOfRange","MaterialNoPath","TextureLoadFailed","TextureSRVNull",
            "NoUVChannel","UVAllZero","SamplerMissing","StillFallbackWhite","StillFallbackMagenta"
        };
        std::string msg = "SubMesh " + std::to_string(submeshIdx) +
            " Issue=" + issueNames[(int)issue] + " | " + detail;
        if (mat && !mat->texName.empty()) msg += " | path=" + mat->texName;
        ErrorLogger::Instance().LogError("TextureBind", msg, false, 1);
    }
}

void ModelRenderComponent::Draw() {
    if (!m_ready || !m_model) return;
    Scene* scene = _Parent->GetParentScene();
    if (!scene) return;
    CameraComponent* cam = scene->GetMainCamera();
    if (!cam) return;

    // �V�F�[�_�� HotReload �Ŗ����ɂȂ����ꍇ�Ď擾
    if (!m_vs || !m_ps) {
        if (!EnsureShaders(false)) return;
    }

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
    ctx->IASetInputLayout(m_layout.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { m_cb.Get() };
    ctx->VSSetConstantBuffers(0, 1, cbs);
    ctx->PSSetConstantBuffers(0, 1, cbs);
    ID3D11SamplerState* smp = s_linearSmp.Get();
    ctx->PSSetSamplers(0, 1, &smp);

    EnsureDebugFallbackTextures();

    for (size_t i = 0; i < m_model->submeshes.size(); ++i) {
        const SubMesh& sm = m_model->submeshes[i];
        size_t matIndex = sm.materialIndex;

        ID3D11ShaderResourceView* srv = s_whiteTexSRV.Get();
        bool usedWhite = true;
        bool usedMagenta = false;
        MaterialRuntime* matPtr = nullptr;

        if (matIndex < m_materials.size()) {
            matPtr = &m_materials[matIndex];
            auto& mat = *matPtr;
            if (!mat.tex && !mat.texName.empty()) {
                mat.tex = TextureManager::Instance()->LoadOrGet(mat.texName);
            }
            if (mat.tex && mat.tex->srv) {
                srv = mat.tex->srv.Get();
                usedWhite = (srv == s_whiteTexSRV.Get());
            }
            else {
                srv = s_magentaTexSRV.Get();
                usedMagenta = true;
                usedWhite = false;
            }
        }
        else {
            srv = s_magentaTexSRV.Get();
            usedMagenta = true;
            usedWhite = false;
        }

        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed(sm.indexCount, sm.indexOffset, 0);

        if (usedWhite || usedMagenta) {
            DiagnoseAndReportTextureIssue(i, sm, matPtr, srv, usedMagenta, usedWhite);
        }
    }
}

void ModelRenderComponent::SaveToFile(std::ostream& out) {
    out << m_modelPath << "\n";
    out << m_color.x << " " << m_color.y << " " << m_color.z << " " << m_color.w << "\n";
    out << m_vsName << "\n" << m_psName << "\n";
}

void ModelRenderComponent::LoadFromFile(std::istream& in) {
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

void ModelRenderComponent::DrawInspector() {
    auto SJ = [](const char* s)->std::string { return EditrGUI::GetInstance()->ShiftJISToUTF8(s); };
    if (!ImGui::CollapsingHeader("ModelRenderComponent", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::Text("%s %s", SJ("���f��:").c_str(), m_modelPath.c_str());
    static char pathBuf[256];
    std::snprintf(pathBuf, sizeof(pathBuf), "%s", m_modelPath.c_str());
    ImGui::InputText("Path", pathBuf, sizeof(pathBuf));
    if (ImGui::Button(SJ("�ēǍ�/�K�p").c_str())) SetModel(pathBuf);

    ImGui::SameLine();
    if (ImGui::Button(SJ("���f���I��...").c_str())) ImGui::OpenPopup("ModelSelectPopup");
    ShowModelSelectPopup();

    if (ImGui::TreeNode(SJ("�V�F�[�_�ݒ�").c_str())) {
        auto* sm = ShaderManager::GetInstance();
        static std::vector<std::string> vsList;
        static std::vector<std::string> psList;
        if (ImGui::Button(SJ("�X�V(��)").c_str())) {
            vsList = sm->GetShaderList("VS");
            psList = sm->GetShaderList("PS");
        }
        if (vsList.empty()) vsList = sm->GetShaderList("VS");
        if (psList.empty()) psList = sm->GetShaderList("PS");

        if (ImGui::BeginCombo("VS", m_vsName.c_str())) {
            for (auto& n : vsList) {
                bool sel = (n == m_vsName);
                if (ImGui::Selectable(n.c_str(), sel)) {
                    m_vsName = n;
                    EnsureShaders(true);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("PS", m_psName.c_str())) {
            for (auto& n : psList) {
                bool sel = (n == m_psName);
                if (ImGui::Selectable(n.c_str(), sel)) {
                    m_psName = n;
                    EnsureShaders(false);
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::TreePop();
    }

    ImGui::ColorEdit4("Color", (float*)&m_color);
    ImGui::Separator();
    ImGui::Text("%s %zu", SJ("�}�e���A����:").c_str(), m_materials.size());

    for (size_t i = 0; i < m_materials.size(); ++i) {
        ImGui::PushID((int)i);
        ImGui::Text("Mat %zu", i);
        ImGui::SameLine();
        std::string shown = m_materials[i].texName.empty() ? SJ("(�Ȃ�)") : m_materials[i].texName;
        ImGui::Text("tex=%s", shown.c_str());
        ImGui::SameLine();
        if (ImGui::Button(SJ("�e�N�X�`���I��...").c_str())) {
            m_texPopupMatIndex = (int)i;
            ImGui::OpenPopup("TextureSelectPopup");
        }
        if (ImGui::BeginPopup("TextureSelectPopup")) {
            auto texList = AssetManager::Instance()->GetCachedTextureNames();
            static char texFilter[128] = "";
            ImGui::InputText("Filter", texFilter, sizeof(texFilter));
            ImGui::BeginChild("TexListChild", ImVec2(380, 230), true);
            for (int ti = 0; ti < (int)texList.size(); ++ti) {
                const std::string& name = texList[ti];
                if (texFilter[0] && name.find(texFilter) == std::string::npos) continue;
                if (ImGui::Selectable(name.c_str(), false)) {
                    m_materials[i].texName = name;
                    m_materials[i].tex.reset();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();
            if (ImGui::Button(SJ("����").c_str())) ImGui::CloseCurrentPopup();
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
        ImGui::InputText(SJ("�t�B���^(������v)").c_str(), filter, sizeof(filter));

        auto list = AssetManager::Instance()->GetCachedAssetNames(true);

        {
            std::string label = SJ("�o�^���f����: ") + std::to_string(list.size());
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
        if (ImGui::Button(SJ("�K�p").c_str()))
        {
            if (currentHighlight >= 0 && currentHighlight < (int)list.size()) {
                const std::string& sel = list[currentHighlight];
                if (SetModel(sel)) m_modelPath = sel;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("�L�����Z��").c_str())) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(SJ("�ă��[�h").c_str())) {
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
        ImGui::InputText(SJ("�t�B���^").c_str(), filter, sizeof(filter));

        auto texList = AssetManager::Instance()->GetCachedTextureNames();
        ImGui::Text(SJ("�e�N�X�`����: %d").c_str(), (int)texList.size());
        ImGui::Separator();

        ImGui::BeginChild("TexList", ImVec2(0, 280), true);
        for (int i = 0; i < (int)texList.size(); ++i) {
            const std::string& name = texList[i];
            if (filter[0] && name.find(filter) == std::string::npos) continue;

            bool select = false;
            if (ImGui::Selectable(name.c_str(), select)) {
                // �I���ő��K�p
                m_materials[materialIndex].texName = name;
                m_materials[materialIndex].tex.reset(); // �ă��[�h������
                m_openTexPopup = false;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();

        if (ImGui::Button(SJ("����").c_str(), ImVec2(80, 0))) {
            m_openTexPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
