#include "ApplicationFeedbackSystem.h"

ApplicationFeedbackSystem* ApplicationFeedbackSystem::instance;

ApplicationFeedbackSystem* ApplicationFeedbackSystem::GetInstance()
{
	if (instance == nullptr) {
		instance = new ApplicationFeedbackSystem();
	}
	return instance;
}

void ApplicationFeedbackSystem::DeleteInstance()
{
	if (instance != nullptr) {
		delete instance;
		instance = nullptr;
	}
}
