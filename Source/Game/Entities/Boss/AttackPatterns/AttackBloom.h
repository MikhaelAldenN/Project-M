#pragma once
#include "IBossAttackPattern.h"
#include "ProjectilePool.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackBloom (Mekar) - Act 2 attack.
//
// Rings of bullets leave Cue, stop at a set distance, hold still while they
// pulse, then fly on in a straight line from where they stopped. Each ring is
// turned half a step from the previous one, so the gaps move. The answer is
// to step into a gap before the ring flies on.
// ============================================================

struct BloomParams {
    int   ringCount = 3;        // USULAN DESAIN
    int   bulletsPerRing = 16;  // USULAN DESAIN
    float ringDelay = 0.5f;     // USULAN DESAIN: seconds between two rings leaving Cue
    float outSpeed = 10.0f;     // USULAN DESAIN: units per second on the way out
    float stopRadius = 8.0f;    // USULAN DESAIN: distance from the ring centre where bullets stop
    float holdDuration = 0.6f;  // USULAN DESAIN: seconds the ring holds still and pulses
    float launchSpeed = 14.0f;  // USULAN DESAIN: units per second after the hold
    float bulletRadius = 0.35f;
    float visualScale = 2.0f;
    int   damage = 3;
};

class AttackBloom : public IBossAttackPattern {
public:
    explicit AttackBloom(const BloomParams& params);
    ~AttackBloom() override = default;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

private:
    struct Ring {
        DirectX::XMFLOAT3 centre{};
        std::vector<ProjectileHandle>  bullets;
        std::vector<DirectX::XMFLOAT2> directions; // unit (x, z), one per bullet
        float age{ 0.0f };                         // seconds since the ring left Cue
    };

    void EmitRing(int ringIndex, const DirectX::XMFLOAT3& centre);
    // Moves the ring through out, hold and launch. Returns false once it has launched.
    bool UpdateRing(Ring& ring, float dt);

    BloomParams     m_params;
    ProjectilePool* m_pool{ nullptr }; // borrowed from the phase, set in Start
    std::vector<Ring> m_rings;         // rings not yet launched
    int   m_ringsEmitted{ 0 };
    float m_ringTimer{ 0.0f };
    bool  m_active{ false };
};
