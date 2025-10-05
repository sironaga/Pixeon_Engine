#ifndef ENGINE_MANAGER_H
#define ENGINE_MANAGER_H

#include <Windows.h>
#include <d3d11.h>
#include <string> 
#include <vector>

class GameRenderTarget;

class EngineManager{
public:
	struct EngineConfig {
		HWND		wnd;
		int			screenWidth;
		int			screenHeight;
		const char* windowTitle;
		bool		fullscreen;
		float		targetFPS;
		const char* startScene;
	};
public:
	static EngineManager* GetInstance();
	static void DeleteInstance();

	int Init(const EngineConfig& InPut);
	void Update();
	void Draw();
	void UnInit();

	// Window Handle éÊìæ
	HWND GetWindowHandle() const { return m_hWnd_; }
	// EditorModeÇÃÇ›óLå¯
	ID3D11ShaderResourceView* GetGameRender();

	// Setter / Getter
	bool IsInGame() const { return m_bInGame_; }
	void SetInGame(bool inGame) { m_bInGame_ = inGame; }
	bool IsShowGUI() const { return m_bIsShowGUI_; }
	void SetShowGUI(bool isShow) { m_bIsShowGUI_ = isShow; }

private:
	void EditeUpdate();
	void InGameUpdate();
	void EditeDraw();
	void InGameDraw();

	EngineManager() {};
	~EngineManager() {};
	
private:
	static EngineManager* instance_;

	HWND m_hWnd_;
	GameRenderTarget* m_gameRenderTarget_;
	// ÉQÅ[ÉÄíÜîªíË	
	bool m_bInGame_;
	// GUIï\é¶îªíË
	bool m_bIsShowGUI_;
};

#endif // !ENGINE_MANAGER_H

