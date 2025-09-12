// シーンクラス
// オブジェクトなどの管理を行う

#pragma once
#include <string>

class Scene
{
public:
	Scene() {}
	virtual ~Scene() {}
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void PlayUpdate();
	virtual void Draw();

public:
	// セーブとロード
	void SaveToFile();
	void LoadToFile();

	void SetName(std::string name) { _name = name; }

private:
	std::string _name = "DefaultScene";
};

