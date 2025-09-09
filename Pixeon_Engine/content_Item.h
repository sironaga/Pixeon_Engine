
#pragma once
//GUI �R���e���c�h�����[�ɕ\�������A�C�e���N���X

#include <string>

class content_Item
{
public:
	enum class content_Item_Kinds {
		MODEL,
		ANIMATION,
		SOUND,
		TEXTURE,
		NONE,
	};

public:
	// �R���X�g���N�^�E�f�X�g���N�^
	content_Item();
	~content_Item();

	void SetKind(content_Item_Kinds k) { kind = k; }
	content_Item_Kinds GetKind() { return kind; }

	void SetViewName(std::string name) { ViewName = name; }
	std::string GetViewName() { return ViewName; }

	void SetFilePath(std::string path) { FilePath = path; }
	std::string GetFilePath() { return FilePath; }

private:
	content_Item_Kinds kind;
	std::string ViewName;
	std::string FilePath;
};

