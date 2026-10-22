#pragma once
#include <DirectXMath.h>

class Camera
{
public:
    Camera();
    ~Camera();

    // --- Transform Operations ---
    void SetPosition(const DirectX::XMFLOAT3& position);
    void SetPosition(float x, float y, float z);
    void Translate(const DirectX::XMFLOAT3& translation);
    void Translate(float x, float y, float z);

    void SetRotation(const DirectX::XMFLOAT3& rotation);
    void SetRotation(float x, float y, float z);
    void LookAt(const DirectX::XMFLOAT3& target);

    // Updates position and rotation to look at a target from a specific eye position
    void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);

    // --- Projection Settings ---
    void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
    void SetAspectRatio(float aspect);
    void SetOffCenterProjection(float left, float right, float bottom, float top, float nearZ, float farZ);
    void SetOrthographic(float viewWidth, float viewHeight, float nearZ, float farZ);

    // --- Accessors ---
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    const DirectX::XMFLOAT4X4& GetView() const { return m_view; }
    const DirectX::XMFLOAT4X4& GetProjection() const { return m_projection; }

    // --- Helpers ---
    DirectX::XMFLOAT3 GetFocus() const;
    const DirectX::XMFLOAT3& GetFront() const { return m_front; }
    const DirectX::XMFLOAT3& GetRight() const { return m_right; }
    const DirectX::XMFLOAT3& GetUp() const { return m_up; }

    bool CheckSphere(float x, float y, float z, float radius);
private:
    void UpdateViewMatrix();

    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_rotation; // Euler angles in Radians

    DirectX::XMFLOAT3 m_front;
    DirectX::XMFLOAT3 m_right;
    DirectX::XMFLOAT3 m_up;

    DirectX::XMFLOAT4X4 m_view;
    DirectX::XMFLOAT4X4 m_projection;

    float m_fovY = DirectX::XM_PIDIV4;
    float m_nearZ = 0.1f;
    float m_farZ = 1000.0f;
};