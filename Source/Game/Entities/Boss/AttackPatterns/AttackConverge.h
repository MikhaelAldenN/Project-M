#pragma once
#include "IBossAttackPattern.h"
#include "ProjectilePool.h"
#include <DirectXMath.h>
#include <memory>
#include <vector>

class Player;
class Primitive;

// ============================================================
// AttackConverge - Act 2 ultimate, gathering part.
//
// Waves of bullets enter from outside the view, from every direction, and
// converge on Cue; they hurt the player on the way. Each wave is announced by
// blinking markers at the screen edge. Bullets that reach Cue are absorbed.
// The boss is held in place for the whole attack.
// ============================================================

struct ConvergeParams {
    int   waves = 5;               // USULAN DESAIN
    int   bulletsPerWave = 12;     // USULAN DESAIN
    float waveDelay = 0.6f;        // seconds between the start of two waves' warnings
    float warningDuration = 0.7f;  // USULAN DESAIN: seconds the edge markers blink before a wave enters
    float travelTime = 1.6f;       // USULAN DESAIN: seconds from entering to reaching Cue, same for every bullet
    float bulletRadius = 0.35f;
    float visualScale = 2.0f;
    float absorbRadius = 1.5f;     // a bullet this close to Cue is absorbed
    int   damage = 3;
};

class AttackConverge : public IBossAttackPattern {
public:
    // `target` is borrowed: the scene owns the player and outlives the attack. Its
    // position stands in for the view centre, since the Act 2 camera follows it.
    AttackConverge(const ConvergeParams& params, Player* target);
    ~AttackConverge() override;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

    [[nodiscard]] const BossMoveTarget* GetBossMoveTarget() const override { return &m_hold; }

    // Bullets that reached Cue so far; the explosion scales with it.
    [[nodiscard]] int GetAbsorbedCount() const { return m_absorbedCount; }

private:
    struct PendingWave {
        std::vector<DirectX::XMFLOAT3> spawnPoints;
        float age{ 0.0f }; // seconds since its markers appeared
    };
    struct IncomingBullet {
        ProjectileHandle handle{};
        float flightTime{ 0.0f };
    };

    // Spawn points of wave `waveIndex`, just outside the view, around `bossPos`.
    std::vector<DirectX::XMFLOAT3> BuildWave(int waveIndex, const DirectX::XMFLOAT3& bossPos) const;
    void LaunchWave(const PendingWave& wave, const DirectX::XMFLOAT3& bossPos);
    // Absorbs bullets that reached Cue and forgets bullets that are gone.
    void UpdateIncoming(float dt, const DirectX::XMFLOAT3& bossPos);

    ConvergeParams  m_params;
    Player* m_target{ nullptr };
    ProjectilePool* m_pool{ nullptr }; // borrowed from the phase, set in Start
    std::unique_ptr<Primitive> m_markerRenderer;

    BossMoveTarget m_hold{};           // the boss's position at Start
    std::vector<PendingWave>    m_pending;
    std::vector<IncomingBullet> m_incoming;
    int   m_wavesQueued{ 0 };
    float m_waveTimer{ 0.0f };
    int   m_absorbedCount{ 0 };
    bool  m_active{ false };
};