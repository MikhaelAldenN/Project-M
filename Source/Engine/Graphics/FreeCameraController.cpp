#include <imgui.h>
#include "FreeCameraController.h"

void FreeCameraController::SyncCameraToController(const Camera& camera)
{
	m_eye = camera.GetPosition();
	m_focus = camera.GetFocus();
	m_up = camera.GetUp();
	m_right = camera.GetRight();

	// Distance from eye to focus
	DirectX::XMVECTOR Eye = DirectX::XMLoadFloat3(&m_eye);
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&m_focus);
	DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(Focus, Eye);
	DirectX::XMVECTOR Distance = DirectX::XMVector3Length(Vec);
	DirectX::XMStoreFloat(&m_distance, Distance);

	// Rotation angles from the front vector
	const DirectX::XMFLOAT3& front = camera.GetFront();
	m_angleX = ::asinf(-front.y);
	if (m_up.y < 0)
	{
		if (front.y > 0)
		{
			m_angleX = -DirectX::XM_PI - m_angleX;
		}
		else
		{
			m_angleX = DirectX::XM_PI - m_angleX;
		}
		m_angleY = ::atan2f(front.x, front.z);
	}
	else
	{
		m_angleY = ::atan2f(-front.x, -front.z);
	}

}

void FreeCameraController::SyncControllerToCamera(Camera& camera)
{
	camera.SetLookAt(m_eye, m_focus, m_up);
}

void FreeCameraController::Update()
{
	// Ignore input while an ImGui window has focus
	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
	{
		return;
	}

	// Camera control uses ImGui's mouse input
	ImGuiIO io = ImGui::GetIO();

	float moveX = io.MouseDelta.x * 0.02f;
	float moveY = io.MouseDelta.y * 0.02f;

	// Right button: orbit
	if (io.MouseDown[ImGuiMouseButton_Right])
	{
		// Yaw
		m_angleY += moveX * 0.5f;
		if (m_angleY > DirectX::XM_PI)
		{
			m_angleY -= DirectX::XM_2PI;
		}
		else if (m_angleY < -DirectX::XM_PI)
		{
			m_angleY += DirectX::XM_2PI;
		}
		// Pitch
		m_angleX += moveY * 0.5f;
		if (m_angleX > DirectX::XM_PI)
		{
			m_angleX -= DirectX::XM_2PI;
		}
		else if (m_angleX < -DirectX::XM_PI)
		{
			m_angleX += DirectX::XM_2PI;
		}
	}
	// Middle button: pan
	else if (io.MouseDown[ImGuiMouseButton_Middle])
	{
		float s = m_distance * 0.035f;
		float x = moveX * s;
		float y = moveY * s;

		m_focus.x -= m_right.x * x;
		m_focus.y -= m_right.y * x;
		m_focus.z -= m_right.z * x;

		m_focus.x += m_up.x * y;
		m_focus.y += m_up.y * y;
		m_focus.z += m_up.z * y;
	}
	// Left and right buttons together: zoom
	else if (io.MouseDown[ImGuiMouseButton_Left] && io.MouseDown[ImGuiMouseButton_Right])
	{
		m_distance += (-moveY - moveX) * m_distance * 0.1f;
	}
	// Mouse wheel: zoom
	else if (io.MouseWheel != 0)
	{
		m_distance -= io.MouseWheel * m_distance * 0.1f;
	}

	float sx = ::sinf(m_angleX);
	float cx = ::cosf(m_angleX);
	float sy = ::sinf(m_angleY);
	float cy = ::cosf(m_angleY);

	// Camera axes
	DirectX::XMVECTOR Front = DirectX::XMVectorSet(-cx * sy, -sx, -cx * cy, 0.0f);
	DirectX::XMVECTOR Right = DirectX::XMVectorSet(cy, 0, -sy, 0.0f);
	DirectX::XMVECTOR Up = DirectX::XMVector3Cross(Right, Front);
	// Eye from focus and distance
	DirectX::XMVECTOR Focus = DirectX::XMLoadFloat3(&m_focus);
	DirectX::XMVECTOR Distance = DirectX::XMVectorSet(m_distance, m_distance, m_distance, 0.0f);
	DirectX::XMVECTOR Eye = DirectX::XMVectorSubtract(Focus, DirectX::XMVectorMultiply(Front, Distance));
	// The transposed view matrix holds the camera's world axes
	DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Eye, Focus, Up);
	DirectX::XMMATRIX World = DirectX::XMMatrixTranspose(View);
	Right = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), World);
	Up = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 1, 0, 0), World);
	DirectX::XMStoreFloat3(&m_eye, Eye);
	DirectX::XMStoreFloat3(&m_up, Up);
	DirectX::XMStoreFloat3(&m_right, Right);
}
