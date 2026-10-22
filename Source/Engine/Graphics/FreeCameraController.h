#pragma once
#include "Camera.h"

class FreeCameraController
{
public:
	// Copies the camera's eye, focus, up and right into the controller.
	void SyncCameraToController(const Camera& camera);

	// Applies the controller's eye, focus and up to the camera.
	void SyncControllerToCamera(Camera& camera);

	// Orbit, pan and zoom from ImGui mouse input.
	void Update();

private:
	DirectX::XMFLOAT3		m_eye;
	DirectX::XMFLOAT3		m_focus;
	DirectX::XMFLOAT3		m_up;
	DirectX::XMFLOAT3		m_right;
	float					m_distance;

	float					m_angleX;
	float					m_angleY;
};
