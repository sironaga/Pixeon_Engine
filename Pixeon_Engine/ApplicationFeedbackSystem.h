#pragma once
// ApplicationFeedbackSystem(AFS)
// �A�v���P�[�V�����Ɋւ���t�B�[�h�o�b�N�𑗐M����V�X�e��
// ��: �N���b�V�����|�[�g�A�g�p�󋵃f�[�^�A�G���[���O�Ȃ�

#include <string>

class ApplicationFeedbackSystem
{
public:
	enum class LogKinds {
		NORMALSTARTUP,
		ABNORMALSTARTUP,
		NORMALOPERATION,
		DRIVEERROR,
		PROCESSINGFAILURE,
		POINTERERROR,
		ABNORMALVAUEDETECTION,
		OTHERS
	};
	struct AFSConfig {
		int days;
		int user;
		int AppID;
		std::string Os;
		LogKinds LogKind;
		std::string LogTxt;
	};
public:
	static ApplicationFeedbackSystem* GetInstance();
	static void DeleteInstance();
public:
	void SendFeedback(const AFSConfig& config);



private:
	static ApplicationFeedbackSystem* instance;
};

