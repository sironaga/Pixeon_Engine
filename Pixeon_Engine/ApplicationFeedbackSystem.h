#pragma once
// ApplicationFeedbackSystem(AFS)
// アプリケーションに関するフィードバックを送信するシステム
// 例: クラッシュレポート、使用状況データ、エラーログなど

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

