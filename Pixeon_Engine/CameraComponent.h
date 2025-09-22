#pragma once
#include "Component.h"

class Object;

class CameraComponent : public Component
{
public:
	CameraComponent()	{}
	~CameraComponent()	{}

	void Init(Object* Prt)	override;
	void EditUpdate()		override;
	void InGameUpdate()		override;





	DirectX::XMFLOAT4X4 GetViewMatrix(bool transpose = true);
	DirectX::XMFLOAT4X4 GetProjectionMatrix(bool transpose = true);
	DirectX::XMMATRIX GetView();
	DirectX::XMMATRIX GetProjection();
	DirectX::XMFLOAT3 GetForwardVector();
	DirectX::XMFLOAT3 GetRightVector();

	DirectX::XMFLOAT3 GetFixation() const			{ return _Fixation; }
	void SetFixation(DirectX::XMFLOAT3 fixation)	{ _Fixation = fixation; }

	// Setter
	void SetFov(float fov)			{ _FOV = fov; }
	void SetAspect(float aspect)	{ _AspectRatio = aspect; }
	void SetNear(float nearPlane)	{ _NearPlane = nearPlane; }
	void SetFar(float farPlane)		{ _FarPlane = farPlane; }
	float GetNear() const			{ return _NearPlane; }
	float GetFar() const			{ return _FarPlane; }
	bool IsMove() const				{ return _IsKeyMove; }
	void SetIsMove(bool isMove)		{ _IsKeyMove = isMove; }
private:
	Object* _Parent;
	DirectX::XMFLOAT3 _Fixation;
	DirectX::XMFLOAT3 _Up;
	float _FOV;
	float _AspectRatio;
	float _NearPlane;
	float _FarPlane;
	float _radius;
	bool _IsKeyMove = false;
};

