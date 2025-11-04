#include <imgui.h>
#include "System/Input.h"
#include "CameraController.h"
#include "Camera.h"

// =============================================================
// CameraController::Update
// - Updates camera behavior based on current control mode
// - Handles input (GamePad / Mouse) and applies camera movement
// - Manages cursor lock toggle (via F1)
// =============================================================
void CameraController::Update(float elapsedTime)
{
    DirectX::XMFLOAT3 eye = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

    switch (controlMode)
    {
    case CameraControlMode::FixedFollow:
        eye.x = target.x + fixedPosition.x;
        eye.y = target.y + fixedPosition.y;
        eye.z = target.z + fixedPosition.z;

        Camera::Instance().SetLookAt(eye, target, up);
        break;

    case CameraControlMode::Free:
    {
        // --- Mouse Look Control ---
        if (toggleCursor)
        {
            float ax = 0.0f;
            float ay = 0.0f;
            float sensitivity = 0.5f;

            Mouse& mouse = Input::Instance().GetMouse();
            ax = static_cast<float>(mouse.GetDeltaX()) * sensitivity;
            ay = static_cast<float>(mouse.GetDeltaY()) * sensitivity;

            float rotSpeed = rollSpeed * elapsedTime;
            angle.x += ay * rotSpeed;
            angle.y += ax * rotSpeed;
        }

        // Clamp & Wrap logic remains outside, operating on the latest angle
        if (angle.x > maxAngleX) { angle.x = maxAngleX; }
        if (angle.x < minAngleX) { angle.x = minAngleX; }
        if (angle.y > DirectX::XM_PI) { angle.y -= DirectX::XM_2PI; }
        if (angle.y < -DirectX::XM_PI) { angle.y += DirectX::XM_2PI; }

        // Transformation and movement logic are always active
        DirectX::XMMATRIX transform = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
        DirectX::XMVECTOR vFront = transform.r[2];
        DirectX::XMVECTOR vRight = transform.r[0];
        DirectX::XMVECTOR vUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        // --- Keyboard Movement Control (Always works) ---
        float moveAmount = moveSpeed * elapsedTime;
        DirectX::XMVECTOR vEye = DirectX::XMLoadFloat3(&this->eye);

        if (GetKeyState('W') & 0x8000) { vEye = DirectX::XMVectorAdd(vEye, DirectX::XMVectorScale(vFront, moveAmount)); }
        if (GetKeyState('S') & 0x8000) { vEye = DirectX::XMVectorSubtract(vEye, DirectX::XMVectorScale(vFront, moveAmount)); }
        if (GetKeyState('A') & 0x8000) { vEye = DirectX::XMVectorSubtract(vEye, DirectX::XMVectorScale(vRight, moveAmount)); }
        if (GetKeyState('D') & 0x8000) { vEye = DirectX::XMVectorAdd(vEye, DirectX::XMVectorScale(vRight, moveAmount)); }
        if (GetKeyState(VK_SPACE) & 0x8000) { vEye = DirectX::XMVectorAdd(vEye, DirectX::XMVectorScale(vUp, moveAmount)); }
        if (GetKeyState(VK_CONTROL) & 0x8000) { vEye = DirectX::XMVectorSubtract(vEye, DirectX::XMVectorScale(vUp, moveAmount)); }

        DirectX::XMStoreFloat3(&this->eye, vEye);

        DirectX::XMVECTOR vTarget = DirectX::XMVectorAdd(vEye, vFront);
        DirectX::XMStoreFloat3(&this->target, vTarget);

        Camera::Instance().SetLookAt(this->eye, this->target, up);
    }
    break;

    case CameraControlMode::GamePad:
    case CameraControlMode::Mouse:
    {
        float ax = 0.0f;
        float ay = 0.0f;
        float sensitivity = 0.5f;

        if (controlMode == CameraControlMode::GamePad) 
        {
            GamePad& gamePad = Input::Instance().GetGamePad();
            ax = gamePad.GetAxisRX();
            ay = gamePad.GetAxisRY();
        }

        else if (controlMode == CameraControlMode::Mouse) 
        {
            if (toggleCursor)
            {
                float sensitivity = 0.5f;
                Mouse& mouse = Input::Instance().GetMouse();
                ax = static_cast<float>(mouse.GetDeltaX()) * sensitivity;
                ay = static_cast<float>(mouse.GetDeltaY()) * sensitivity;
            }
        }

        float speed = rollSpeed * elapsedTime;
        angle.x += ay * speed;
        angle.y += ax * speed;

        if (angle.x > maxAngleX) { angle.x = maxAngleX; }
        if (angle.x < minAngleX) { angle.x = minAngleX; }

        if (angle.y > DirectX::XM_PI) { angle.y -= DirectX::XM_2PI; }
        if (angle.y < -DirectX::XM_PI) { angle.y += DirectX::XM_2PI; }

        DirectX::XMMATRIX Transform = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
        DirectX::XMVECTOR Front = Transform.r[2];

        DirectX::XMFLOAT3 front;
        DirectX::XMStoreFloat3(&front, Front);

        eye.x = target.x - front.x * range;
        eye.y = target.y - front.y * range;
        eye.z = target.z - front.z * range;

        Camera::Instance().SetLookAt(eye, target, up);
    }
    break;
    }

    // --- Cursor Locking logic -----
    static bool isF1Pressed = false;
    if (GetKeyState(VK_F1) & 0x8000)
    {
        if (!isF1Pressed)
        {
            // Hanya berlaku jika mode Mouse atau Free aktif
            if (controlMode == CameraControlMode::Mouse || controlMode == CameraControlMode::Free)
            {
                toggleCursor = !toggleCursor; // Membalik status (true -> false, false -> true)
            }
            isF1Pressed = true;
        }
    }
    else
    {
        isF1Pressed = false;
    }

    if (controlMode == CameraControlMode::Mouse || controlMode == CameraControlMode::Free) {
        Input::Instance().GetMouse().LockCursor(toggleCursor);
    }
    else {
        Input::Instance().GetMouse().LockCursor(false);
    }
}

// =============================================================
// CameraController::DrawDebugGUI
// - Draws ImGui debug window for camera settings
// - Allows switching camera control modes and editing parameters
// =============================================================
void CameraController::DrawDebugGUI()
{
    ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
    ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_FirstUseEver);
    ImGui::SetWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera", nullptr, ImGuiWindowFlags_None))
    {
        if (ImGui::RadioButton("Fixed Follow", controlMode == CameraControlMode::FixedFollow)) {
            SetControlMode(CameraControlMode::FixedFollow);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("GamePad", controlMode == CameraControlMode::GamePad)) {
            SetControlMode(CameraControlMode::GamePad);
        }

        if (ImGui::RadioButton("Mouse", controlMode == CameraControlMode::Mouse)) {
            SetControlMode(CameraControlMode::Mouse);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Free", controlMode == CameraControlMode::Free)) {
            SetControlMode(CameraControlMode::Free);
        }

        if (controlMode == CameraControlMode::FixedFollow) {
            ImGui::Separator();
            ImGui::Text("Fixed Follow Settings");
            ImGui::InputFloat3("Position", &fixedPosition.x);
        }

        if (controlMode == CameraControlMode::GamePad) {
            ImGui::Separator();
            ImGui::Text("Press [IJKL] to control camera");
        }

        if (controlMode == CameraControlMode::Mouse) {
            ImGui::Separator();
            ImGui::Text("Press [F1] to toggle cursor");
        }

        if (controlMode == CameraControlMode::Free) {
            ImGui::Separator();
            ImGui::Text("Press [F1] to toggle cursor");
            ImGui::Text("[WASD] to move");
            ImGui::Text("[Space] / [Ctrl] to go up/down");
            ImGui::InputFloat3("Position", &this->eye.x, "%.2f", ImGuiInputTextFlags_ReadOnly);
            //ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f);
        }
    }
    ImGui::End();
}