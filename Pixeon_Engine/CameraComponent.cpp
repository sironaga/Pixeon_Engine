#include <fstream>
#include <sstream>
#include <iostream>
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

	if (_IsKeyMove) {
		// カメラ操作
	}

	_Position.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + _Fixation.x;
	_Position.y = sinf(_Rotation.y) * _radius + _Fixation.y;
	_Position.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + _Fixation.z;
}

void CameraComponent::InGameUpdate(){
	if (_IsKeyMove) {
		// カメラ操作
	}

	_Position.x = cosf(_Rotation.y) * sinf(_Rotation.x) * _radius + _Fixation.x;
	_Position.y = sinf(_Rotation.y) * _radius + _Fixation.y;
	_Position.z = cosf(_Rotation.y) * cosf(_Rotation.x) * _radius + _Fixation.z;
}

void CameraComponent::DrawInspector(){
	std::string label = EditrGUI::GetInstance()->ShiftJISToUTF8("CameraComponent");
	std::string Ptr = std::to_string((uintptr_t)this);
	label += "###" + Ptr;
	if (ImGui::CollapsingHeader(EditrGUI::GetInstance()->ShiftJISToUTF8(label).c_str())) {

	}
}

void CameraComponent::SaveToFile(std::ostream& out){
	out << _Position.x << " " << _Position.y << " " << _Position.z << " ";
	out << _Rotation.x << " " << _Rotation.y << " " << _Rotation.z << " ";
	out << _Fixation.x << " " << _Fixation.y << " " << _Fixation.z << " ";
	out << _Up.x << " " << _Up.y << " " << _Up.z << " ";
	out << _FOV << " " << _AspectRatio << " " << _NearPlane << " " << _FarPlane << " ";
	out << _radius << " ";
	out << _IsKeyMove << " ";
}

void CameraComponent::LoadFromFile(std::istream& in){
	in >> _Position.x >> _Position.y >> _Position.z;
	in >> _Rotation.x >> _Rotation.y >> _Rotation.z;
	in >> _Fixation.x >> _Fixation.y >> _Fixation.z;
	in >> _Up.x >> _Up.y >> _Up.z;
	in >> _FOV >> _AspectRatio >> _NearPlane >> _FarPlane;
	in >> _radius;
	in >> _IsKeyMove;
}

DirectX::XMFLOAT4X4 CameraComponent::GetViewMatrix(bool transpose){
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMMATRIX View;

	DirectX::XMVECTOR Eye = DirectX::XMVectorSet(_Fixation.x, _Fixation.y, _Fixation.z, 0.0f);
	DirectX::XMVECTOR At = DirectX::XMVectorSet(_Position.x, _Position.y, _Position.z, 0.0f);
	DirectX::XMVECTOR Up = DirectX::XMVectorSet(_Up.x, _Up.y, _Up.z, 0.0f);

	View = DirectX::XMMatrixLookAtLH(Eye, At, Up);

	if (transpose) View = DirectX::XMMatrixTranspose(View);
	DirectX::XMStoreFloat4x4(&Mat, View);
	return Mat;
}

DirectX::XMFLOAT4X4 CameraComponent::GetProjectionMatrix(bool transpose){
	DirectX::XMFLOAT4X4 Mat;
	DirectX::XMMATRIX Proj;

	Proj = DirectX::XMMatrixPerspectiveFovLH(_FOV,_AspectRatio,_NearPlane,_FarPlane);

	if (transpose) Proj = DirectX::XMMatrixTranspose(Proj);
	DirectX::XMStoreFloat4x4(&Mat, Proj);
	return Mat;
}

DirectX::XMMATRIX CameraComponent::GetView(){
	DirectX::XMVECTOR Eye	= DirectX::XMVectorSet(_Position.x,_Position.y, _Position.z, 0.0f);
	DirectX::XMVECTOR At	= DirectX::XMVectorSet(_Fixation.x, _Fixation.y, _Fixation.z, 0.0f);
	DirectX::XMVECTOR Up	= DirectX::XMVectorSet(_Up.x, _Up.y, _Up.z, 0.0f);

	return DirectX::XMMatrixLookAtLH(Eye, At, Up);
}

DirectX::XMMATRIX CameraComponent::GetProjection(){
	DirectX::XMMATRIX Proj = DirectX::XMMatrixPerspectiveFovLH(_FOV, _AspectRatio, _NearPlane, _FarPlane);
	return Proj;
}

DirectX::XMFLOAT3 CameraComponent::GetForwardVector(){
	DirectX::XMFLOAT3 forward;
	forward.x = cosf(_Rotation.y) * sinf(_Rotation.x);
	forward.y = sinf(_Rotation.y);
	forward.z = cosf(_Rotation.y) * cosf(_Rotation.x);

	float len = sqrtf(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
	if (len != 0.0f)
	{
		forward.x /= len;
		forward.y /= len;
		forward.z /= len;
	}
	return forward;
}

DirectX::XMFLOAT3 CameraComponent::GetRightVector(){
	// 上方向はワールドのY軸
	DirectX::XMFLOAT3 up = _Up;
	DirectX::XMFLOAT3 forward = GetForwardVector();

	// 外積 right = up × forward
	DirectX::XMFLOAT3 right;
	right.x = up.y * forward.z - up.z * forward.y;
	right.y = up.z * forward.x - up.x * forward.z;
	right.z = up.x * forward.y - up.y * forward.x;

	// 正規化
	float len = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
	if (len != 0.0f)
	{
		right.x /= len;
		right.y /= len;
		right.z /= len;
	}

	return right;
}
