// [New file: Source/Game/Entities/Boss/AttackPatterns/AttackClock.h]
#pragma once
#include "IBossAttackPattern.h"
#include "ProjectileMesh.h"
#include "ProjectilePool.h"
#include <DirectXMath.h>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class Player;
class Primitive;
class Camera;

// ============================================================
// AttackClock - Act 3 ultimate.
//
// Cue glides sideways to the horizontal centre of the screen and stays still there
// until the attack ends. Cue's face becomes a clock. Three hands (hour, minute, second)
// turn clockwise at different speeds and fire along themselves at different intervals;
// bullet speeds vary along a fixed sequence. The clock then stops and every bullet
// freezes. The hands turn back and every bullet flies back along its own line, for as
// long as it flew out, to the point it left, while the screen shakes. Once all are home,
// a shockwave expands from the clock centre. Bullets may leave the screen; nothing marks them.
// ============================================================

struct ClockHandParams {
    float turnSpeed = 90.0f;   // degrees per second, clockwise seen from above
    float fireInterval = 0.2f; // seconds between two bullets of this hand
    float length = 3.0f;       // units from the centre to the tip, where bullets leave
};

struct ClockParams {
    std::array<ClockHandParams, 3> hands{ {
        { 30.0f, 0.5f, 2.0f },  // USULAN DESAIN: hour hand
        { 90.0f, 0.2f, 3.0f },  // USULAN DESAIN: minute hand
        { 270.0f, 0.1f, 3.5f }, // USULAN DESAIN: second hand
    } };
    float approachDuration = 0.8f; // USULAN DESAIN: seconds Cue takes to glide to x = 0
    float spinDuration = 5.0f;   // USULAN DESAIN: seconds the hands turn and fire (about 85 bullets)
    float freezeDuration = 0.8f; // USULAN DESAIN: seconds the clock and the bullets stand still
    float bulletSpeedMin = 6.0f; // USULAN DESAIN: units per second; a bullet keeps its speed out and back
    float bulletSpeedMax = 12.0f;// USULAN DESAIN
    float rewindTrauma = 1.0f;   // USULAN DESAIN: camera trauma added per second while bullets fly home
    float bulletRadius = 0.35f;
    float visualScale = 2.0f;
    int   damage = 10;
    float blastDuration = 0.35f; // USULAN DESAIN: seconds for the shockwave to reach full radius
    float blastRadius = 9.0f;    // USULAN DESAIN
};

class AttackClock : public IBossAttackPattern {
public:
    // `target` is borrowed: the scene owns the player and outlives the attack.
    AttackClock(const ClockParams& params, Player* target);
    ~AttackClock() override;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

    // Holds Cue at the screen centre from Start until the attack is done.
    [[nodiscard]] const BossMoveTarget* GetBossMoveTarget() const override;

private:
    enum class Stage : std::uint8_t {
        approach, // Cue glides to the centre, nothing fires yet
        spin,   // hands turn clockwise and fire
        freeze, // hands and bullets stand still
        rewind, // hands turn back, bullets fly home
        blast,  // the shockwave expands and hurts
        done,
    };

    struct ClockBullet {
        ProjectileHandle handle{};
        float flightTime{ 0.0f }; // seconds flown outward; the way home takes the same time
    };

    void UpdateSpin(float dt);
    void UpdateRewind(float dt);
    void UpdateBlast();
    void BeginRewind();
    void FireBullet(const ClockHandParams& hand);
    // Drops bullets that already hit the player.
    void ForgetGoneBullets();

    // Clockwise angle from 12 o'clock, in degrees, for the current stage.
    [[nodiscard]] float HandAngle(const ClockHandParams& hand) const;
    void RenderBullets() const;
    void DrawHands(ID3D11DeviceContext* context, const Camera& camera);
    void DrawRing(ID3D11DeviceContext* context, const Camera& camera, float radius,
        float width, const DirectX::XMFLOAT4& color);

    ClockParams m_params;
    Player* m_target{ nullptr };
    ProjectilePool* m_pool{ nullptr }; // borrowed from the phase, set in Start
    ProjectileMesh m_mesh;
    std::unique_ptr<Primitive> m_lineRenderer;

    BossMoveTarget m_hold{};            // the screen centre; lerpSpeed = 1 / approachDuration
    DirectX::XMFLOAT3 m_centre{};       // Cue's position at Start; the clock does not follow its sway
    std::vector<ClockBullet> m_bullets;
    std::array<float, 3> m_fireTimers{}; // seconds until each hand fires again

    Stage m_stage{ Stage::done };
    float m_stageTimer{ 0.0f };  // seconds since the current stage began
    float m_spinElapsed{ 0.0f }; // how long the hands actually turned
    int   m_bulletsFired{ 0 };   // index into the speed sequence
};