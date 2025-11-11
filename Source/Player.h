#pragma once

#include "System/ModelRenderer.h"
#include "Character.h"
#include "ProjectileManager.h"
#include "StateMachine.h"
#include "Animation.h"

/// Player
/// - Main player character; handles input, movement, projectiles
class Player : public Character
{
public:
    Player();
    ~Player() override;

    void Update(float elapsedTime);
    void Render(const RenderContext& rc, ModelRenderer* renderer);

    void InputMove(float elapsedTime);
    void InputJump();
    void InputProjectile();

    void Jump(float speed);

    void DrawDebugGUI();
    void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);

    // === FSM ===
    StateMachine* GetStateMachine() { return stateMachine; }
    bool IsRunning() const;
    bool IsSprinting() const;

    void SetMoveSpeed(float speed) { moveSpeed = speed; }
    float GetMoveSpeed() const { return moveSpeed; }

    bool IsOnGround() const { return position.y <= 0.0f; }

public:
    Animation* GetAnimation() const { return animation; }
    Model* GetModel() const { return model; }

private:
    // Helper Methods
    DirectX::XMFLOAT3 GetMoveVec() const;
    void CollisionPlayerVsEnemies();
    void CollisionProjectilesVsEnemies();

private:
    // Player Data Members
    Model* model = nullptr;
    Animation* animation;
    float moveSpeed = 7.0f;
    float turnSpeed = DirectX::XMConvertToRadians(720);
    float jumpSpeed = 12.0f;

    ProjectileManager projectileManager;
    StateMachine* stateMachine = nullptr;
};