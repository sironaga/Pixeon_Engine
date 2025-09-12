// シーンクラス
// オブジェクトなどの管理を行う

#pragma once
#include <string>

class Scene
{
public:



	void SetName(std::string name) { _name = name; }

private:
	std::string _name = "DefaultScene";
};

