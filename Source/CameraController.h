#pragma once

#include <DirectXMath.h>
#include "Camera.h"

/// =============================================================
/// CameraControlMode
/// - Defines how the camera is controlled
/// =============================================================
enum class CameraControlMode
{
    GamePad,
    Mouse,
    FixedFollow,
    Free
};

/// =============================================================
/// CameraController (Singleton)
/// - Manages camera movement and input handling
/// - Supports GamePad, Mouse, and Fixed Follow modes
/// =============================================================
class CameraController
{
private:
    // =================================================================
    // Singleton Pattern: Private constructor and destructor
    // =================================================================
    CameraController() {}
    ~CameraController() {}

    // =================================================================
    // Data Members
    // =================================================================
    DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };

    // Members for Free Camera mode
    DirectX::XMFLOAT3 eye = { 0.0f, 5.0f, -10.0f };
    float moveSpeed = 15.0f;

    float rollSpeed = DirectX::XMConvertToRadians(90);
    float range = 10.0f;
    float maxAngleX = DirectX::XMConvertToRadians(85);
    float minAngleX = DirectX::XMConvertToRadians(-85);

    CameraControlMode controlMode = CameraControlMode::GamePad;
    DirectX::XMFLOAT3 fixedPosition = { 0.0f, 15.0f, -10.0f };

public:
    // =================================================================
    // Singleton Instance Access
    // =================================================================
    static CameraController& Instance()
    {
        static CameraController instance;
        return instance;
    }

    // =================================================================
    // Camera Control Methods
    // =================================================================
    void Update(float elapsedTime);
    void DrawDebugGUI();

    void SetTarget(const DirectX::XMFLOAT3& target) { this->target = target; }
    
    void SetControlMode(CameraControlMode mode)
    {
        if (this->controlMode != CameraControlMode::Mouse && mode == CameraControlMode::Mouse)
        {
			toggleCursor = true;
        }

        if (this->controlMode != CameraControlMode::Free && mode == CameraControlMode::Free)
        {
            this->eye = Camera::Instance().GetEye();
			toggleCursor = true;
        }
        this->controlMode = mode;
    }

    CameraControlMode GetControlMode() const { return controlMode; }    

    bool toggleCursor = true;
};