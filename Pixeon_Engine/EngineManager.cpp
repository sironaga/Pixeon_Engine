#include "EngineManager.h"
#include "System.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "ResourceService.h"
#include "AssetManager.h"
#include "SceneManger.h"
#include "SettingManager.h"
#include "ShaderManager.h"
#include "ComponentManager.h"
#include "Object.h"

EngineManager* EngineManager::instance_ = nullptr;

EngineManager* EngineManager::GetInstance(){
	if (instance_ == nullptr){
		instance_ = new EngineManager();
	}
	return instance_;
}

void EngineManager::DeleteInstance(){
	if (instance_ != nullptr){
		delete instance_;
		instance_ = nullptr;
	}
}


int EngineManager::Init(const EngineConfig& InPut){

	m_bInGame_		= false;
	m_bIsShowGUI_	= true;

	// COM �̏�����
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr)) return -1;
	m_hWnd_ = InPut.wnd;
	// DirectX11 ������
	hr = DirectX11::GetInstance()->Init(InPut.wnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) {
		CoUninitialize();
		return -1;
	}
	// �ݒ�ǂݍ���
	SettingManager::GetInstance()->LoadConfig();
	// AssetManager ������
	AssetManager::Instance()->SetRoot(SettingManager::GetInstance()->GetAssetsFilePath());
	AssetManager::Instance()->SetLoadMode(AssetManager::LoadMode::FromSource);
	AssetManager::Instance()->StartAutoSync(std::chrono::milliseconds(1000), true);
	// �G���W���p�����_�[�e�N�X�`��������
	m_gameRenderTarget_ = new GameRenderTarget();
	m_gameRenderTarget_->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
	// GUI ������
	EditrGUI::GetInstance()->Init();
	// �V�[���}�l�[�W���[������
	SceneManger::GetInstance()->Init();
	// �V�F�[�_�[������
	ShaderManager::GetInstance()->Initialize(DirectX11::GetInstance()->GetDevice());
	// �R���|�[�l���g������
	ComponentManager::GetInstance()->Init();
	return 0;
}

void EngineManager::Update() {
	if (m_bInGame_)
		InGameUpdate();
	else
		EditeUpdate();
}

void EngineManager::Draw() {
	m_gameRenderTarget_->SetRenderZBuffer(SettingManager::GetInstance()->GetZBuffer());
	if (!m_bIsShowGUI_)
		InGameDraw();
	else
		EditeDraw();	
}

void EngineManager::UnInit() {
	// AssetManager �̎���������~
	AssetManager::Instance()->StopAutoSync();
	// �ۑ�
	SceneManger::GetInstance()->Save();
	SettingManager::GetInstance()->SaveConfig();
	// �j������
	AssetManager::DeleteInstance();
	ComponentManager::DestroyInstance();
	SceneManger::DestroyInstance();
	SettingManager::DestroyInstance();
	ShaderManager::DestroyInstance();
	TextureManager::DeleteInstance();
	ModelManager::DeleteInstance();
	ResourceService::DeleteInstance();
	DirectX11::GetInstance()->Uninit();
	DirectX11::DestroyInstance();
	CoUninitialize();
}


ID3D11ShaderResourceView* EngineManager::GetGameRender(){
	return m_gameRenderTarget_->GetShaderResourceView();
}


void EngineManager::EditeUpdate() {
	ShaderManager::GetInstance()->UpdateAndCompileShaders();
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
}

void EngineManager::InGameUpdate() {
	SceneManger::GetInstance()->PlayUpdate();
}

void EngineManager::EditeDraw() {
	m_gameRenderTarget_->Begin(DirectX11::GetInstance()->GetContext());
	SceneManger::GetInstance()->Draw();
	m_gameRenderTarget_->End();

	ID3D11DeviceContext* ctx = DirectX11::GetInstance()->GetContext();
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	ctx->PSSetShaderResources(0, 1, nullSRV);

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void EngineManager::InGameDraw() {
	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}