#pragma once

#include "System/ModelRenderer.h"
#include "Character.h"
#include "ProjectileManager.h"
#include "StateMachine.h"

/// =============================================================
/// Player
/// - Main player character; handles input, movement, projectiles
/// =============================================================
class Player : public Character
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    Player();
    ~Player() override;

    // =================================================================
    // Main Logic Methods
    // =================================================================
    void Update(float elapsedTime);
    void Render(const RenderContext& rc, ModelRenderer* renderer);

    // =================================================================
    // Input Methods
    // =================================================================
    void InputMove(float elapsedTime);
    void InputJump();
    void InputProjectile();

    // =================================================================
    // Movement/Actions
    // =================================================================
    void Jump(float speed);

    // =================================================================
    // Debug/GUI
    // =================================================================
    void DrawDebugGUI();
    void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);

    // === FSM ===
    StateMachine* GetStateMachine() { return stateMachine; }
    bool IsMoving() const;

private:
    // =================================================================
    // Helper Methods
    // =================================================================
    DirectX::XMFLOAT3 GetMoveVec() const;
    void CollisionPlayerVsEnemies();
    void CollisionProjectilesVsEnemies();

private:
    // =================================================================
    // Player Data Members
    // =================================================================
    Model* model = nullptr;
    float moveSpeed = 5.0f;
    float turnSpeed = DirectX::XMConvertToRadians(720);
    float jumpSpeed = 12.0f;

    ProjectileManager projectileManager;
    StateMachine* stateMachine = nullptr;
};