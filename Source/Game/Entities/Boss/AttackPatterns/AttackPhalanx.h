#pragma once
#include "IBossAttackPattern.h"
#include "ProjectilePool.h"
#include <DirectXMath.h>
#include <random>

// ============================================================
// AttackPhalanx - Phase 1 attack pattern.
//
// Boss slides beside the player (left or right), into the arena half
// its companion Rain leaves free, summons N orbiting homing bullets in
// a spread fan facing the player, holds briefly, then launches them one
// by one. Fired bullets home only briefly, then fly straight.
//
// States:
//   1 = Moving + Charging (spawning bullets one by one)
//   2 = Holding (all bullets orbit, waiting before fire)
//   3 = Firing (launch one bullet at a time)
//   4 = Post-fire pause; the attack ends where the boss stands
// ============================================================

struct PhalanxParams {
    int   count = 8;
    float chargeDelay = 0.2f;   // Delay between each bullet spawn
    float holdDuration = 2.5f;   // How long bullets orbit before firing
    float fireDelay = 0.05f;  // Delay between each bullet launch
    float speed = 70.0f;
    float hoverRadius = 4.0f;   // Orbit radius around boss
    float turnSpeed = 3.0f;   // Homing turn speed after launch
    float smoothSpeed = 12.0f;  // Orbit slot lerp speed
    float postFireDelay = 1.0f;
    float attackMoveSpeed = 3.0f;   // Boss lateral move speed during attack
    float returnMoveSpeed = 2.0f;
    int   damage = 3;
    float sfxVolume = 1.0f;
    float homingDuration = 0.5f; // USULAN DESAIN: seconds a fired bullet keeps steering toward the player
};

class Player;

class AttackPhalanx : public IBossAttackPattern {
public:
    AttackPhalanx(const PhalanxParams& params, Player* target);
    ~AttackPhalanx() override = default;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

    [[nodiscard]] const BossMoveTarget* GetBossMoveTarget() const override { return &m_moveTarget; }

    // The arena half (above or below the centre) the companion Rain covers; the boss
    // takes the other half. Call before Start; defaults to the bottom half.
    void SetRainHalf(bool isPositiveZ) { m_isRainOnPositiveZ = isPositiveZ; }

private:
    void TriggerRainCallback(); // Calls back into the phase to trigger rain

    // Ages fired bullets: stops their homing after homingDuration and removes them
    // after the maximum flight time.
    void UpdateFiredBullets(float dt);
    // Stops homing on every fired bullet still alive and forgets them; they fly
    // straight until they hit or leave the arena.
    void ReleaseFiredBullets();

    PhalanxParams                         m_params;
    Player* m_target = nullptr;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    CueBoss* m_boss = nullptr;
    bool m_isRainOnPositiveZ{ false };

    int   m_state = 0;
    float m_timer = 0.0f;
    int   m_spawned = 0;
    int   m_fired = 0;
    bool  m_flareTriggered = false;

    BossMoveTarget m_moveTarget{}; // set in Start

    std::vector<ProjectileHandle> m_bullets; // projectiles this attack spawned, in firing order

    struct FiredBullet {
        ProjectileHandle handle{};
        float flightTime{ 0.0f }; // seconds since launch
    };
    std::vector<FiredBullet> m_firedBullets; // launched bullets this attack still steers
};