#include "EditrGUI.h"

EditrGUI* EditrGUI::instance = nullptr;	

EditrGUI* EditrGUI::GetInstance()
{
	if (instance == nullptr) {
		instance = new EditrGUI();
	}
	return instance;
}

void EditrGUI::DestroyInstance()
{
	if (instance) {
		delete instance;
		instance = nullptr;
	}
}

void EditrGUI::Init(){
}

void EditrGUI::Update(){
}

void EditrGUI::Draw(){
}
