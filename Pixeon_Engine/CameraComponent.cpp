#include "CameraComponent.h"
#include "Object.h"

void CameraComponent::Init(Object* Prt){
	_Parent = Prt;
	_Type			= ComponentManager::COMPONENT_TYPE::CAMERA;
	_Fixation		= { 0.0f, 5.0f, 0.0f };
	_Up				= { 0.0f, 1.0f, 0.0f };
	_FOV			= DirectX::XMConvertToRadians(60.0f);
	_AspectRatio	= 16.0f / 9.0f;
	_NearPlane		= 30.0f * 0.01f;
	_FarPlane		= 1000.0f;
	_radius			= 5.0f;
}


void CameraComponent::EditUpdate(){

	Transform Edit = _Parent->GetTransform();
	if (_IsKeyMove) {
		// ƒJƒƒ‰‘€ì
	}

	Edit.position.x = cosf(Edit.rotation.y) * sinf(Edit.rotation.x) * _radius + _Fixation.x;
	Edit.position.y = sinf(Edit.rotation.y) * _radius + _Fixation.y;
	Edit.position.z = cosf(Edit.rotation.y) * cosf(Edit.rotation.x) * _radius + _Fixation.z;

	_Parent->SetTransform(Edit);
}

void CameraComponent::InGameUpdate(){
	Transform Edit = _Parent->GetTransform();
	if (_IsKeyMove) {
		// ƒJƒƒ‰‘€ì
	}

	Edit.position.x = cosf(Edit.rotation.y) * sinf(Edit.rotation.x) * _radius + _Fixation.x;
	Edit.position.y = sinf(Edit.rotation.y) * _radius + _Fixation.y;
	Edit.position.z = cosf(Edit.rotation.y) * cosf(Edit.rotation.x) * _radius + _Fixation.z;

	_Parent->SetTransform(Edit);
}
