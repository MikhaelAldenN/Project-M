#pragma once
#include "IBossAttackPattern.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackMeteor - Phase 1 attack pattern.
//
// Fires large bullets one after another. With the default anchors they
// cross the screen diagonally from the top-right corner to the bottom-left.
// ============================================================

struct MeteorParams {
    int   count = 5;
    float spawnDelay = 0.4f;
    float speed = 35.0f;
    float speedVariance = 10.0f; // Random speed range
    float radius = 0.8f;
    float visualScale = 3.0f;

    float startX = 24.0f;
    float startZ = 15.0f;
    float targetX = -24.0f;
    float targetZ = -15.0f;

    float spreadOffset = 4.0f;
    int   damage = 3;
    float sfxVolume = 1.0f;

    int   triggerCount = 1;
    float triggerDelay = 1.0f;
}; 

class AttackMeteor :public IBossAttackPattern {
public:
    explicit AttackMeteor(const MeteorParams& params);
    ~AttackMeteor() override = default;

    void Start(Boss* boss, ProjectilePool* pool) override;
    void Update(float dt, Boss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) override;
    void Stop(Boss* boss) override;

    bool IsFinished() const override;

private:
    void FireMeteor();

    MeteorParams                          m_params;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    Boss* m_boss = nullptr;

    bool  m_active = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;

    int   m_currentTrigger = 0;
    float m_triggerTimer = 0.0f;
    bool  m_isTriggerWaiting = false;
};