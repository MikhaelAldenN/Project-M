#include "Camera.h"

// SetLookAt: Set camera position, focus, and up vector
// - Updates view matrix and camera vectors
void Camera::SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up)
{
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&eye);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&focus);
	DirectX::XMVECTOR Up = DirectX::XMLoadFloat3(&up);
	DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixLookAtLH(Eye, Focus, Up));

	// Extract the right, up, and front vectors from the view matrix
	this->right.x = view._11;
	this->right.y = view._21;
	this->right.z = view._31;

	this->up.x = view._12;
	this->up.y = view._22;
	this->up.z = view._32;

	this->front.x = view._13;
	this->front.y = view._23;
	this->front.z = view._33;

	// Store the eye and focus positions
	this->eye = eye;
	this->focus = focus;
}

// SetPerspectiveFov: Set perspective projection matrix
void Camera::SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ)
{
	DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
	DirectX::XMStoreFloat4x4(&projection, Projection);
}