
#pragma once
//GUI コンテンツドロワーに表示されるアイテムクラス

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
	// コンストラクタ・デストラクタ
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

