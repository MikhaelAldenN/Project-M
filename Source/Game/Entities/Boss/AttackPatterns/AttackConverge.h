#pragma once
#include "IBossAttackPattern.h"
#include "ProjectilePool.h"
#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <vector>

class Player;
class Primitive;
class Camera;

// ============================================================
// AttackConverge - Act 2 ultimate.
//
// Waves of bullets enter from outside the arena and the view, from every
// direction, and converge on Cue; they hurt the player on the way. Bullets
// that reach Cue are absorbed. Cue then charges, with the blast area drawn on
// the floor, and releases a shockwave that hurts the player inside it. The
// blast grows with the absorbed bullets. The boss is held in place throughout.
// ============================================================

struct ConvergeParams {
    int   waves = 5;                    // USULAN DESAIN
    int   bulletsPerWave = 12;          // USULAN DESAIN
    float waveDelay = 0.6f;             // seconds between two waves entering
    float travelTime = 1.6f;            // USULAN DESAIN: seconds from entering to reaching Cue, same for every bullet
    float bulletRadius = 0.35f;
    float visualScale = 2.0f;
    float absorbRadius = 1.5f;          // a bullet this close to Cue is absorbed
    int   damage = 3;

    float chargeDuration = 1.0f;        // USULAN DESAIN: seconds the blast area shows before the blast
    float blastDuration = 0.35f;        // USULAN DESAIN: seconds for the shockwave to reach full radius
    float blastRadiusBase = 10.0f;      // USULAN DESAIN
    float blastRadiusPerBullet = 0.12f; // USULAN DESAIN: added per absorbed bullet
    float blastRadiusMax = 20.0f;
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

private:
    enum class Stage : std::uint8_t {
        gather, // waves enter and converge on Cue
        charge, // the blast area shows on the floor
        blast,  // the shockwave expands and hurts
        done,
    };

    struct IncomingBullet {
        ProjectileHandle handle{};
        float flightTime{ 0.0f };
    };

    void UpdateGather(float dt, const DirectX::XMFLOAT3& bossPos);
    void UpdateBlast();
    void LaunchWave(int waveIndex, const DirectX::XMFLOAT3& bossPos);
    // Absorbs bullets that reached Cue and forgets bullets that are gone.
    void UpdateIncoming(float dt, const DirectX::XMFLOAT3& bossPos);
    [[nodiscard]] float BlastRadius() const;
    void DrawGroundRing(ID3D11DeviceContext* context, const Camera& camera, float radius,
        float width, const DirectX::XMFLOAT4& color);

    ConvergeParams  m_params;
    Player* m_target{ nullptr };
    ProjectilePool* m_pool{ nullptr }; // borrowed from the phase, set in Start
    std::unique_ptr<Primitive> m_ringRenderer;

    BossMoveTarget m_hold{};           // the boss's position at Start
    std::vector<IncomingBullet> m_incoming;
    Stage m_stage{ Stage::done };
    float m_stageTimer{ 0.0f };        // seconds since the current stage began
    int   m_wavesLaunched{ 0 };
    float m_waveTimer{ 0.0f };
    int   m_absorbedCount{ 0 };
};