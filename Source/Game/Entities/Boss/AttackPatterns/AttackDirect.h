#pragma once
#include "IBossAttackPattern.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackDirect - Phase 1 attack pattern.
//
// Fires bullets one at a time straight at the player. The aim is
// taken again for every bullet, so the stream follows the player.
// ============================================================

struct DirectParams {
    int   count = 10;           // bullets per trigger
    float spawnDelay = 0.2f;    // seconds between shots
    float speed = 45.0f;
    float radius = 0.35f;       // hitbox radius
    float visualScale = 1.5f;
    int   damage = 2;
    float sfxVolume = 1.0f;

    int   triggerCount = 1;     // used by the combo AI
    float triggerDelay = 1.0f;
};

class Player;

class AttackDirect :public IBossAttackPattern {
public:
    AttackDirect(const DirectParams& params, Player* target);
    ~AttackDirect() override = default;

    void Start(Boss* boss, ProjectilePool* pool) override;
    void Update(float dt, Boss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) override;
    void Stop(Boss* boss) override;

    bool IsFinished() const override;

private:
    void FireBullet();

    DirectParams                          m_params;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    Boss* m_boss = nullptr;
    Player* m_target = nullptr; // Tracking target

    bool  m_active = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;

    int   m_currentTrigger = 0;
    float m_triggerTimer = 0.0f;
    bool  m_isTriggerWaiting = false;
};