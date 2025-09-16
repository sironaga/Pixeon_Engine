//エディタ用GUIクラス

#pragma once
#include <string>

class EditrGUI
{
public:
	static EditrGUI* GetInstance();
	static void DestroyInstance();

	void Init();
	void Update();
	void Draw();

public:
	std::string ShiftJISToUTF8(const std::string& str);

private:
	void WindowGUI();

	void ShowContentDrawer();
	void ShowHierarchy();
	void ShowInspector();
	void ShowGameView();
	void ShowConsole();

	void SettingWindow();

	bool dockNeedsReset = false;
	bool ShowSettingsWindow = false;
	bool ShowConsoleWindow = false;
private:
	static EditrGUI* instance;
private:
	EditrGUI() {}
	~EditrGUI() {}
};

