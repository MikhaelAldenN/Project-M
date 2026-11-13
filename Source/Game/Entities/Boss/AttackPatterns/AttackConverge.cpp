#include "AttackConverge.h"
#include "CueBoss.h"
#include "Player.h"
#include "Primitive.h"
#include "Camera.h"
#include "CameraController.h"
#include "System/Graphics.h"
#include "Engine/Common/Constants.h"
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace
{
    constexpr float k_goldenAngle{ 2.39996323f };  // radians; rotates each wave so directions never repeat
    constexpr float k_offscreenMargin{ 2.0f };     // units beyond the arena and view edges where bullets enter
    constexpr float k_maxExtraFlight{ 1.0f };      // seconds past travelTime before a bullet is removed
    constexpr int   k_ringSegments{ 48 };
    constexpr XMFLOAT4 k_areaColor{ 1.0f, 0.2f, 0.2f, 1.0f }; // USULAN DESAIN

    // Distance along (dirX, dirZ) from `from` to the edge of the box around `centre`.
    // 0 when `from` is already outside the box in that direction.
    float DistanceToBoxEdge(const XMFLOAT3& from, float dirX, float dirZ,
        const XMFLOAT2& centre, float halfX, float halfZ)
    {
        float distance{ FLT_MAX };
        if (dirX > 1e-4f)  distance = min(distance, (centre.x + halfX - from.x) / dirX);
        if (dirX < -1e-4f) distance = min(distance, (centre.x - halfX - from.x) / dirX);
        if (dirZ > 1e-4f)  distance = min(distance, (centre.y + halfZ - from.z) / dirZ);
        if (dirZ < -1e-4f) distance = min(distance, (centre.y - halfZ - from.z) / dirZ);
        return (distance == FLT_MAX) ? 0.0f : max(distance, 0.0f);
    }
}

AttackConverge::AttackConverge(const ConvergeParams& params, Player* target)
    : m_params(params), m_target(target) {}

// Defined here so the unique_ptr can destroy Primitive, which the header only declares.
AttackConverge::~AttackConverge() = default;

void AttackConverge::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackConverge needs the phase's bullet pool");
    m_pool = pool;
    m_ringRenderer = std::make_unique<Primitive>(Graphics::Instance().GetDevice());

    m_hold.position = boss ? boss->GetPosition() : XMFLOAT3{};
    m_hold.lerpSpeed = 2.0f;

    m_incoming.clear();
    m_stage = Stage::gather;
    m_stageTimer = 0.0f;
    m_wavesLaunched = 0;
    m_waveTimer = m_params.waveDelay; // the first wave enters at once
    m_absorbedCount = 0;
}

void AttackConverge::Update(float dt, CueBoss* boss) {
    if (m_stage == Stage::done || !m_pool || !boss) return;

    m_stageTimer += dt;
    switch (m_stage) {
    case Stage::gather:
        UpdateGather(dt, boss->GetPosition());
        break;

    case Stage::charge:
        if (m_stageTimer >= m_params.chargeDuration) {
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

void AttackConverge::UpdateGather(float dt, const XMFLOAT3& bossPos) {
    m_waveTimer += dt;
    if (m_wavesLaunched < m_params.waves && m_waveTimer >= m_params.waveDelay) {
        m_waveTimer -= m_params.waveDelay;
        LaunchWave(m_wavesLaunched, bossPos);
        ++m_wavesLaunched;
    }

    UpdateIncoming(dt, bossPos);
    if (m_wavesLaunched >= m_params.waves && m_incoming.empty()) {
        m_stage = Stage::charge;
        m_stageTimer = 0.0f;
    }
}

void AttackConverge::UpdateBlast() {
    const float progress{ (m_params.blastDuration > 0.0f)
        ? min(m_stageTimer / m_params.blastDuration, 1.0f) : 1.0f };
    const float radius{ BlastRadius() * progress };

    if (m_target && m_target->GetHP() > 0) {
        const XMFLOAT3 playerPos{ m_target->GetPosition() };
        const float dx{ playerPos.x - m_hold.position.x };
        const float dz{ playerPos.z - m_hold.position.z };
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

void AttackConverge::LaunchWave(int waveIndex, const XMFLOAT3& bossPos) {
    if (m_params.bulletsPerWave <= 0) return;

    const XMFLOAT3 playerPos{ m_target ? m_target->GetPosition() : bossPos };
    const XMFLOAT2 arenaCentre{ 0.0f, 0.0f };
    const XMFLOAT2 viewCentre{ playerPos.x, playerPos.z };
    const float arenaHalfX{ Beyond::Config::ACT2_ARENA_WIDTH_UNITS * 0.5f + k_offscreenMargin };
    const float arenaHalfZ{ Beyond::Config::ACT2_ARENA_HEIGHT_UNITS * 0.5f + k_offscreenMargin };
    const float viewHalfX{ Beyond::Config::ARENA_WIDTH_UNITS * 0.5f + k_offscreenMargin };
    const float viewHalfZ{ Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f + k_offscreenMargin };

    const float step{ XM_2PI / static_cast<float>(m_params.bulletsPerWave) };
    const float offset{ std::fmod(static_cast<float>(waveIndex) * k_goldenAngle, step) };

    for (int i{ 0 }; i < m_params.bulletsPerWave; ++i) {
        const float angle{ offset + step * static_cast<float>(i) };
        const float dirX{ std::sin(angle) };
        const float dirZ{ std::cos(angle) };
        // Why the larger of the two: near an arena edge the view reaches past the arena,
        // and a bullet must never appear inside either.
        const float distance{ max(
            DistanceToBoxEdge(bossPos, dirX, dirZ, arenaCentre, arenaHalfX, arenaHalfZ),
            DistanceToBoxEdge(bossPos, dirX, dirZ, viewCentre, viewHalfX, viewHalfZ)) };
        const XMFLOAT3 spawn{ bossPos.x + dirX * distance, bossPos.y, bossPos.z + dirZ * distance };

        const ProjectileHandle handle{ m_pool->Spawn() };
        Projectile* projectile{ m_pool->Get(handle) };
        // Why: a full pool leaves a gap in the ring; the attack still ends on schedule.
        if (!projectile) continue;

        // Why speed from distance: every bullet of the wave reaches Cue at the same moment.
        const float speed{ (m_params.travelTime > 0.0f) ? distance / m_params.travelTime : distance };
        projectile->radius = m_params.bulletRadius;
        projectile->visualScale = m_params.visualScale;
        projectile->damage = m_params.damage;
        ProjectilePool::Launch(*projectile, spawn, { -dirX, 0.0f, -dirZ }, speed);
        m_incoming.push_back({ handle, 0.0f });
    }
}

void AttackConverge::UpdateIncoming(float dt, const XMFLOAT3& bossPos) {
    const float absorbRadiusSq{ m_params.absorbRadius * m_params.absorbRadius };
    for (auto it = m_incoming.begin(); it != m_incoming.end(); ) {
        Projectile* projectile{ m_pool->Get(it->handle) };
        // Gone already: it hit the player.
        if (!projectile) {
            it = m_incoming.erase(it);
            continue;
        }

        it->flightTime += dt;
        const float dx{ projectile->position.x - bossPos.x };
        const float dz{ projectile->position.z - bossPos.z };
        const bool isAbsorbed{ dx * dx + dz * dz <= absorbRadiusSq };
        // Why the time limit: a bullet that passes Cue between frames must not fly on.
        const bool isOverdue{ it->flightTime >= m_params.travelTime + k_maxExtraFlight };
        if (isAbsorbed || isOverdue) {
            if (isAbsorbed) ++m_absorbedCount;
            m_pool->Despawn(it->handle);
            it = m_incoming.erase(it);
            continue;
        }
        ++it;
    }
}

float AttackConverge::BlastRadius() const {
    const float radius{ m_params.blastRadiusBase
        + m_params.blastRadiusPerBullet * static_cast<float>(m_absorbedCount) };
    return min(radius, m_params.blastRadiusMax);
}

void AttackConverge::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    if (!m_ringRenderer || !camera) return;
    if (m_stage != Stage::charge && m_stage != Stage::blast) return;

    const float fullRadius{ BlastRadius() };
    if (m_stage == Stage::charge) {
        // Pulses three times a second while Cue charges.
        const float pulse{ (std::sin(m_stageTimer * XM_2PI * 3.0f) + 1.0f) * 0.5f };
        XMFLOAT4 color{ k_areaColor };
        color.w = 0.4f + 0.6f * pulse;
        DrawGroundRing(context, *camera, fullRadius, 4.0f, color);
    }
    else {
        const float progress{ (m_params.blastDuration > 0.0f)
            ? min(m_stageTimer / m_params.blastDuration, 1.0f) : 1.0f };
        XMFLOAT4 faint{ k_areaColor };
        faint.w = 0.3f;
        DrawGroundRing(context, *camera, fullRadius, 2.0f, faint);
        DrawGroundRing(context, *camera, fullRadius * progress, 10.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
    }
    m_ringRenderer->Render(context);
}

void AttackConverge::DrawGroundRing(ID3D11DeviceContext* context, const Camera& camera, float radius,
    float width, const XMFLOAT4& color)
{
    if (radius <= 0.0f) return;

    const XMFLOAT3& centre{ m_hold.position };
    XMFLOAT2 previous{ WorldToViewportPixels(context, camera, { centre.x + radius, 0.0f, centre.z }) };
    for (int i{ 1 }; i <= k_ringSegments; ++i) {
        const float angle{ XM_2PI * static_cast<float>(i) / static_cast<float>(k_ringSegments) };
        const XMFLOAT2 current{ WorldToViewportPixels(context, camera,
            { centre.x + std::cos(angle) * radius, 0.0f, centre.z + std::sin(angle) * radius }) };
        m_ringRenderer->Line(previous.x, previous.y, current.x, current.y, width,
            color.x, color.y, color.z, color.w);
        previous = current;
    }
}

void AttackConverge::Stop(CueBoss* boss) {
    if (m_pool) {
        for (const IncomingBullet& bullet : m_incoming) {
            m_pool->Despawn(bullet.handle);
        }
    }
    m_incoming.clear();
    m_stage = Stage::done;
}

bool AttackConverge::IsFinished() const {
    return m_stage == Stage::done;
}