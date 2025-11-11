#pragma once

#include <DirectXMath.h>
#include "System/ShapeRenderer.h"

// This is the base class for all character entities
class Character
{
public:
    Character() {};
    virtual ~Character() {};

	// Accessors and Mutators
    void UpdateTransform();
    const DirectX::XMFLOAT3& GetPosition() const { return position; }
    void SetPosition(const DirectX::XMFLOAT3& position) { this->position = position; }
    const DirectX::XMFLOAT3& GetAngle() const { return angle; }
    void SetAngle(const DirectX::XMFLOAT3& angle) { this->angle = angle; }
    const DirectX::XMFLOAT3& GetScale() const { return scale; }
    void SetScale(const DirectX::XMFLOAT3& scale) { this->scale = scale; }
    float GetRadius() const { return radius; }
    float GetHeight() const { return height; }
    const DirectX::XMFLOAT3& GetVelocity() const { return velocity; }

	// Damage Handling
    void AddImpulse(const DirectX::XMFLOAT3& impulse);
    bool ApplyDamage(int damage, float invincibleTime);

    virtual void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);

private:
    // Internal Movement/Physics Helpers
    void UpdateVerticalVelocity(float elapsedTime);
    void UpdateVerticalMove(float elapsedTime);
    void UpdateHorizontalVelocity(float elapsedTime);
    void UpdateHorizontalMove(float elapsedTime);

protected:
    // Movement/Actions
// TODO: Might want to remove or change these for the new Player States implementation
    void Move(float elapsedTime, float vx, float vy, float speed);
    void Turn(float elapsedTime, float vx, float vz, float speed);
    void Jump(float speed);

    void UpdateVelocity(float elapsedTime);
    void UpdateInvincibleTimer(float elapsedTime);

    // Event Hooks (Override in Derived)
    virtual void OnLanding() {}
    virtual void OnDamaged() {}
    virtual void OnDead() {}

protected:
    // Character Data Members
    DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4X4 transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    DirectX::XMFLOAT3 velocity = { 0, 0, 0 };
    float acceleration = 50.0f;
    float maxMoveSpeed = 5.0f;
    float moveVecX = 0.0f;
    float moveVecZ = 0.0f;
    float friction = 15.0f;
   
    float gravity = -30.0f;
    float airControl = 0.3f;
    bool isGround = false;

    float radius = 0.5f;
    float height = 2.0f;

    int health = 5;
    float invincibleTimer = 1.0f;
};