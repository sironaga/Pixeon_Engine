#include "Main.h"
#include "System.h"
#include "AssetsManager.h"
#include "SceneManger.h"
#include "GameRenderTarget.h"
#include "EditrGUI.h"
#include "PostEffectBase.h"
#include "SettingManager.h"

HWND ghWnd;
AssetWatcher *watcher;
GameRenderTarget* gGameRenderTarget;
bool bInGame;
std::vector<PostEffectBase*> gPostEffects;

int Init(const EngineConfig& InPut){
	// DirectX11の初期化
	ghWnd = InPut.hWnd;
	HRESULT hr = DirectX11::GetInstance()->Init(InPut.hWnd, InPut.screenWidth, InPut.screenHeight, InPut.fullscreen);
	if (FAILED(hr)) return -1;
	//　アセットマネージャーの起動
	AssetsManager::GetInstance()->Open("assets.PixAssets");
	watcher = new AssetWatcher(".", "assets.PixAssets",
		[&]() {
			AssetsManager::GetInstance()->Open("assets.PixAssets");
		}
	);
	watcher->Start();

	gGameRenderTarget = new GameRenderTarget();
	gGameRenderTarget->Init(DirectX11::GetInstance()->GetDevice(), InPut.screenWidth, InPut.screenHeight);

	EditrGUI::GetInstance()->Init();
	bInGame = false;

	SceneManger::GetInstance()->Init();
	SettingManager::GetInstance()->LoadConfig();

	return 0;
}

void Update(){
	if(bInGame){
		InGameUpdate();
	}
	else{
		EditeUpdate();
	}
}

void Draw(){
	EditeDraw();
}

void UnInit(){
	watcher->Stop();
	SettingManager::GetInstance()->SaveConfig();
	DirectX11::DestroyInstance();
}

// 内部処理

void EditeUpdate(){
	EditrGUI::GetInstance()->Update();
	SceneManger::GetInstance()->EditUpdate();
}

void InGameUpdate(){
	SceneManger::GetInstance()->PlayUpdate();
}

void EditeDraw(){

	gGameRenderTarget->Begin(DirectX11::GetInstance()->GetContext());
	SceneManger::GetInstance()->Draw();
	gGameRenderTarget->End();

	DirectX11::GetInstance()->BeginDraw();
	EditrGUI::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void InGamDraw(){

	DirectX11::GetInstance()->BeginDraw();
	SceneManger::GetInstance()->Draw();
	DirectX11::GetInstance()->EndDraw();
}

void AssetsUpdate() {

}

HWND GetWindowHandle(){
	return ghWnd;
}

ID3D11ShaderResourceView* GetGameRender(){
	return gGameRenderTarget->GetShaderResourceView();
}
