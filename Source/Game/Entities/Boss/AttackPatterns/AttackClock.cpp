// [New file: Source/Game/Entities/Boss/AttackPatterns/AttackClock.cpp]
#include "AttackClock.h"
#include "CueBoss.h"
#include "Player.h"
#include "Primitive.h"
#include "Camera.h"
#include "CameraController.h"
#include "System/Graphics.h"
#include <algorithm>
#include <cassert>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr int      k_ringSegments{ 48 };
    constexpr float    k_arriveDistance{ 0.05f };                   // units; Cue counts as arrived
    constexpr float    k_approachGrace{ 0.5f };                     // seconds past approachDuration before the hands start anyway
    constexpr float    k_handWidth{ 6.0f };                        // pixels
    constexpr XMFLOAT4 k_handColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr XMFLOAT4 k_bulletColor{ 1.0f, 0.85f, 0.2f, 1.0f };   // USULAN DESAIN
    constexpr XMFLOAT4 k_blastColor{ 1.0f, 0.2f, 0.2f, 1.0f };     // USULAN DESAIN
    constexpr float    k_speedStep{ 0.61803399f };                  // golden ratio: speeds never repeat in a visible order

    // Unit vector for a clockwise angle from +Z (12 o'clock), seen from above.
    XMFLOAT3 HandDirection(float angleDegrees)
    {
        const float radians{ XMConvertToRadians(angleDegrees) };
        return { std::sin(radians), 0.0f, std::cos(radians) };
    }

    // Fractional part of index * step: an even spread over [0, 1) with no visible order.
    float SequenceValue(int index, float step)
    {
        const float value{ static_cast<float>(index) * step };
        return value - std::floor(value);
    }
}

AttackClock::AttackClock(const ClockParams& params, Player* target)
    : m_params(params), m_target(target) {}

// Defined here so the unique_ptr can destroy Primitive, which the header only declares.
AttackClock::~AttackClock() = default;

void AttackClock::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackClock needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");
    m_lineRenderer = std::make_unique<Primitive>(Graphics::Instance().GetDevice());

    // The clock runs at x = 0 and Cue's depth at Start. Cue glides there and stays still until
    // the attack ends (BossPhase02 follows GetBossMoveTarget), so every bullet comes home to the
    // point it left.
    const float startZ{ boss ? boss->GetPosition().z : 0.0f };
    m_centre = XMFLOAT3{ 0.0f, 0.0f, startZ };
    m_hold.position = m_centre;
    m_hold.lerpSpeed = (m_params.approachDuration > 0.0f) ? 1.0f / m_params.approachDuration : 1000.0f;

    m_bullets.clear();
    m_bulletsFired = 0;
    // Why one interval: the three hands do not all fire at 12 o'clock on the first frame.
    for (std::size_t i{ 0 }; i < m_fireTimers.size(); ++i) {
        m_fireTimers[i] = m_params.hands[i].fireInterval;
    }
    m_stage = Stage::approach;
    m_stageTimer = 0.0f;
    m_spinElapsed = 0.0f;
}

void AttackClock::Update(float dt, CueBoss* boss) {
    if (m_stage == Stage::done || !m_pool) return;

    m_stageTimer += dt;
    switch (m_stage) {
    case Stage::approach: {
        const XMFLOAT3 bossPos{ boss ? boss->GetPosition() : m_centre };
        const float dx{ bossPos.x - m_centre.x };
        const float dz{ bossPos.z - m_centre.z };
        const bool hasArrived{ dx * dx + dz * dz <= k_arriveDistance * k_arriveDistance };
        // Why the time limit: the hands still start if the phase does not move Cue.
        if (hasArrived || m_stageTimer >= m_params.approachDuration + k_approachGrace) {
            m_stage = Stage::spin;
            m_stageTimer = 0.0f;
        }
        break;
    }

    case Stage::spin:
        UpdateSpin(dt);
        if (m_stageTimer >= m_params.spinDuration) {
            m_spinElapsed = m_stageTimer;
            m_stage = Stage::freeze;
            m_stageTimer = 0.0f;
        }
        break;

    case Stage::freeze:
        ForgetGoneBullets();
        if (m_stageTimer >= m_params.freezeDuration) {
            BeginRewind();
        }
        break;

    case Stage::rewind:
        UpdateRewind(dt);
        // Why per second: the shake stays the same strength at any frame rate.
        CameraController::Instance().AddTrauma(m_params.rewindTrauma * dt);
        // The hands are back at 12 o'clock after as long as they turned forward.
            if (m_bullets.empty() && m_stageTimer >= m_spinElapsed) {
            m_stage = Stage::blast;
            m_stageTimer = 0.0f;
            CameraController::Instance().AddTrauma(0.8f);
        }
        break;

    case Stage::blast:
        UpdateBlast();
        break;

    case Stage::done:
        break;
    }
}

void AttackClock::UpdateSpin(float dt) {
    for (auto it = m_bullets.begin(); it != m_bullets.end(); ) {
        Projectile* projectile{ m_pool->Get(it->handle) };
        // Gone already: it hit the player.
        if (!projectile) {
            it = m_bullets.erase(it);
            continue;
        }
        ProjectilePool::Advance(*projectile, dt);
        it->flightTime += dt;
        ++it;
    }

    for (std::size_t i{ 0 }; i < m_params.hands.size(); ++i) {
        const ClockHandParams& hand{ m_params.hands[i] };
        if (hand.fireInterval <= 0.0f) continue;

        m_fireTimers[i] -= dt;
        while (m_fireTimers[i] <= 0.0f) {
            FireBullet(hand);
            m_fireTimers[i] += hand.fireInterval;
        }
    }
}

void AttackClock::FireBullet(const ClockHandParams& hand) {
    const ProjectileHandle handle{ m_pool->Spawn() };
    Projectile* projectile{ m_pool->Get(handle) };
    // Why: a full pool skips this bullet; the clock still runs on schedule.
    if (!projectile) return;

    const XMFLOAT3 direction{ HandDirection(HandAngle(hand)) };
    const XMFLOAT3 tip{
        m_centre.x + direction.x * hand.length,
        m_centre.y,
        m_centre.z + direction.z * hand.length };

    const float speed{ m_params.bulletSpeedMin
        + (m_params.bulletSpeedMax - m_params.bulletSpeedMin) * SequenceValue(m_bulletsFired, k_speedStep) };
    ++m_bulletsFired;

    projectile->radius = m_params.bulletRadius;
    projectile->visualScale = m_params.visualScale;
    projectile->damage = m_params.damage;
    ProjectilePool::Launch(*projectile, tip, direction, speed);
    m_bullets.push_back({ handle, 0.0f });
}

void AttackClock::ForgetGoneBullets() {
    m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
        [this](const ClockBullet& bullet) { return m_pool->Get(bullet.handle) == nullptr; }),
        m_bullets.end());
}

void AttackClock::BeginRewind() {
    ForgetGoneBullets();
    for (const ClockBullet& bullet : m_bullets) {
        if (Projectile * projectile{ m_pool->Get(bullet.handle) }) {
            projectile->velocity = { -projectile->velocity.x, 0.0f, -projectile->velocity.z };
        }
    }
    m_stage = Stage::rewind;
    m_stageTimer = 0.0f;
}

void AttackClock::UpdateRewind(float dt) {
    for (auto it = m_bullets.begin(); it != m_bullets.end(); ) {
        Projectile* projectile{ m_pool->Get(it->handle) };
        if (!projectile) {
            it = m_bullets.erase(it);
            continue;
        }

        // Flies back exactly as long as it flew out, so it stops on the point it left.
        const float step{ (std::min)(dt, it->flightTime) };
        ProjectilePool::Advance(*projectile, step);
        it->flightTime -= step;
        if (it->flightTime <= 0.0f) {
            m_pool->Despawn(it->handle);
            it = m_bullets.erase(it);
            continue;
        }
        ++it;
    }
}

void AttackClock::UpdateBlast() {
    const float progress{ (m_params.blastDuration > 0.0f)
        ? (std::min)(m_stageTimer / m_params.blastDuration, 1.0f) : 1.0f };
    const float radius{ m_params.blastRadius * progress };

    if (m_target && m_target->GetHP() > 0) {
        const XMFLOAT3 playerPos{ m_target->GetPosition() };
        const float dx{ playerPos.x - m_centre.x };
        const float dz{ playerPos.z - m_centre.z };
        // Why every frame: Player::TakeDamage ignores hits during i-frames and turns a
        // dash through the shockwave into a perfect dodge, the same as a bullet.
        if (dx * dx + dz * dz <= radius * radius) {
            m_target->TakeDamage(DamageInfo{ m_params.damage, playerPos });
        }
    }

    if (progress >= 1.0f) {
        m_stage = Stage::done;
    }
}

float AttackClock::HandAngle(const ClockHandParams& hand) const {
    switch (m_stage) {
    case Stage::spin:   return hand.turnSpeed * m_stageTimer;
    case Stage::freeze: return hand.turnSpeed * m_spinElapsed;
    case Stage::rewind: return hand.turnSpeed * (std::max)(m_spinElapsed - m_stageTimer, 0.0f);
    default:            return 0.0f;
    }
}

void AttackClock::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    if (!camera) return;
    RenderBullets();

    if (!m_lineRenderer) return;
    switch (m_stage) {
    case Stage::spin:
    case Stage::freeze:
    case Stage::rewind:
        DrawHands(context, *camera);
        break;

    case Stage::blast: {
        const float progress{ (m_params.blastDuration > 0.0f)
            ? (std::min)(m_stageTimer / m_params.blastDuration, 1.0f) : 1.0f };
        XMFLOAT4 faint{ k_blastColor };
        faint.w = 0.3f;
        DrawRing(context, *camera, m_params.blastRadius, 2.0f, faint);
        DrawRing(context, *camera, m_params.blastRadius * progress, 10.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
        break;
    }

    case Stage::approach:
    case Stage::done:
        return;
    }
    m_lineRenderer->Render(context);
}

void AttackClock::RenderBullets() const {
    if (!m_pool || !m_mesh.IsLoaded()) return;
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    for (const ClockBullet& bullet : m_bullets) {
        if (const Projectile * projectile{ m_pool->Get(bullet.handle) }) {
            m_mesh.Draw(*modelRenderer, ShaderId::Phong, k_bulletColor,
                projectile->position, projectile->visualScale, 0.0f);
        }
    }
}

void AttackClock::DrawHands(ID3D11DeviceContext* context, const Camera& camera) {
    const XMFLOAT2 centre{ WorldToViewportPixels(context, camera, m_centre) };
    for (const ClockHandParams& hand : m_params.hands) {
        const XMFLOAT3 direction{ HandDirection(HandAngle(hand)) };
        const XMFLOAT2 tip{ WorldToViewportPixels(context, camera, {
            m_centre.x + direction.x * hand.length,
            m_centre.y,
            m_centre.z + direction.z * hand.length }) };
        m_lineRenderer->Line(centre.x, centre.y, tip.x, tip.y, k_handWidth,
            k_handColor.x, k_handColor.y, k_handColor.z, k_handColor.w);
    }
}

void AttackClock::DrawRing(ID3D11DeviceContext* context, const Camera& camera, float radius,
    float width, const XMFLOAT4& color)
{
    if (radius <= 0.0f) return;

    XMFLOAT2 previous{ WorldToViewportPixels(context, camera, { m_centre.x + radius, 0.0f, m_centre.z }) };
    for (int i{ 1 }; i <= k_ringSegments; ++i) {
        const float angle{ XM_2PI * static_cast<float>(i) / static_cast<float>(k_ringSegments) };
        const XMFLOAT2 current{ WorldToViewportPixels(context, camera,
            { m_centre.x + std::cos(angle) * radius, 0.0f, m_centre.z + std::sin(angle) * radius }) };
        m_lineRenderer->Line(previous.x, previous.y, current.x, current.y, width,
            color.x, color.y, color.z, color.w);
        previous = current;
    }
}

void AttackClock::Stop(CueBoss* boss) {
    if (m_pool) {
        for (const ClockBullet& bullet : m_bullets) {
            m_pool->Despawn(bullet.handle);
        }
    }
    m_bullets.clear();
    m_stage = Stage::done;
}

const BossMoveTarget* AttackClock::GetBossMoveTarget() const {
    return (m_stage == Stage::done) ? nullptr : &m_hold;
}

bool AttackClock::IsFinished() const {
    return m_stage == Stage::done;
}