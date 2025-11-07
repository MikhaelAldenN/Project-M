#pragma once

#include <DirectXMath.h>

/// Camera (Singleton)
/// - Manages view/projection matrices and camera vectors
class Camera
{
private:
    Camera() {}
    ~Camera() {}

    // Data Members
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT3 eye;
    DirectX::XMFLOAT3 focus;
    DirectX::XMFLOAT3 up;
    DirectX::XMFLOAT3 front;
    DirectX::XMFLOAT3 right;

public:
    static Camera& Instance()
    {
        static Camera instance;
        return instance;
    }

    // Accessors
    const DirectX::XMFLOAT3& GetEye() const { return eye; }
    const DirectX::XMFLOAT3& GetFocus() const { return focus; }
    const DirectX::XMFLOAT3& GetUp() const { return up; }
    const DirectX::XMFLOAT3& GetFront() const { return front; }
    const DirectX::XMFLOAT3& GetRight() const { return right; }
    const DirectX::XMFLOAT4X4& GetView() const { return view; }
    const DirectX::XMFLOAT4X4& GetProjection() const { return projection; }

    // Camera Utility Methods
    void SetLookAt(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& focus, const DirectX::XMFLOAT3& up);
    void SetPerspectiveFov(float fovY, float aspect, float nearZ, float farZ);
};