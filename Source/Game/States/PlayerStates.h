#pragma once
#include "PlayerState.h"
#include "PlayerConstants.h"
#include <DirectXMath.h>

class Player;

// ============================================================
// STATE DECLARATIONS
// Implementations live in PlayerStates.cpp
// ============================================================

class PlayerIdle : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerMoving : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

class PlayerDash : public PlayerState
{
public:
    void Enter(Player* player) override;
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override;
private:
    float m_timer = 0.0f;
    DirectX::XMFLOAT2 m_dashDir = { 0.0f, 0.0f };
    float m_speedScale{ 1.0f }; // below 1 on a penalized dash, set in Enter
    bool m_isDashBuffered{ false }; // dash pressed during this dash, runs when it ends
    int m_dashGoVfxHandle = -1;
};

// Held shoot input: fires through Player::TryFire, which owns the fire interval.
class PlayerShoot : public PlayerState
{
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override;
    void Exit(Player* player) override {}
};

// TODO: implement damage and death behaviour
class PlayerDamage : public PlayerState
{
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override {}
    void Exit(Player* player) override {}
};

class PlayerDead : public PlayerState
{
public:
    void Enter(Player* player) override {}
    void Update(Player* player, float dt) override {}
    void Exit(Player* player) override {}
};