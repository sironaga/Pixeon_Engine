// �V�[���N���X
// �I�u�W�F�N�g�Ȃǂ̊Ǘ����s��

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

public: // �I�u�W�F�N�g�̒ǉ��ƍ폜
	bool AddObject(Object* obj);
public: // �Z�[�u�ƃ��[�h
	void SaveToFile();
	void LoadToFile();
public: // Setter And Getter
	void SetName(std::string name) { _name = name; }
	std::string GetName() { return _name; }
	// ���ׂẴI�u�W�F�N�g���擾
	std::vector<Object*> GetObjects() { return _objects; }
private://��������
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

