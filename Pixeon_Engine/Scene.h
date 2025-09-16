// シーンクラス
// オブジェクトなどの管理を行う

#pragma once
#include <string>
#include <vector>
#include <mutex>

class Object;

class Scene
{
public:
	Scene() {}
	virtual ~Scene();
	virtual void Init();
	virtual void BeginPlay();
	virtual void EditUpdate();
	virtual void PlayUpdate();
	virtual void Draw();

public: // オブジェクトの追加と削除
	bool AddObject(Object* obj);
public: // セーブとロード
	void SaveToFile();
	void LoadToFile();
public: // Setter And Getter
	void SetName(std::string name) { _name = name; }
	std::string GetName() { return _name; }
	// すべてのオブジェクトを取得
	std::vector<Object*> GetObjects() { return _objects; }
private://内部処理
	void ProcessThreadSafeAdditions();

private:
	std::string _name = "DefaultScene";

	std::vector<Object*> _objects;
	std::vector<Object*> _SaveObjects;
	std::vector<Object*> _ToBeRemoved;
	std::vector<Object*> _ToBeAdded;
	std::vector<Object*> _ToBeAddedBuffer;
	std::mutex _mtx;

};

