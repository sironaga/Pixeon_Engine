//�G�f�B�^�pGUI�N���X

#pragma once

class EditrGUI
{
public:
	static EditrGUI* GetInstance();
	static void DestroyInstance();

	void Init();
	void Update();
	void Draw();


private:
	static EditrGUI* instance;
private:
	EditrGUI() {}
	~EditrGUI() {}
};

