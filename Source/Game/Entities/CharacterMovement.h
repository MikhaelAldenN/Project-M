#pragma once
#include <DirectXMath.h>

// Pure data container for character transform state.
// Collision against the stage is resolved by the owner (Player uses KinematicMover).
class CharacterMovement
{
public:
    CharacterMovement() = default;
    ~CharacterMovement() = default;

    // --- Position ---
    const DirectX::XMFLOAT3& GetPosition() const { return m_position; }
    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }

    // --- Rotation ---
    const DirectX::XMFLOAT3& GetRotation() const { return m_rotation; }
    void SetRotation(const DirectX::XMFLOAT3& rot) { m_rotation = rot; }
    float GetRotationY() const { return m_rotation.y; }
    void  SetRotationY(float rad) { m_rotation.y = rad; }

    // --- Velocity ---
    const DirectX::XMFLOAT3& GetVelocity() const { return m_velocity; }
    void SetVelocity(const DirectX::XMFLOAT3& vel) { m_velocity = vel; }
    void SetVelocityX(float x) { m_velocity.x = x; }
    void SetVelocityY(float y) { m_velocity.y = y; }
    void SetVelocityZ(float z) { m_velocity.z = z; }

    // Returns true if character has meaningful horizontal movement
    bool IsMoving() const;

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_velocity = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotation = { 0.0f, 0.0f, 0.0f };
};