#include "AttackConverge.h"
#include "CueBoss.h"
#include "Player.h"
#include "Primitive.h"
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
    constexpr float k_offscreenMargin{ 2.0f };     // units beyond the view edge where bullets enter
    constexpr float k_markerSize{ 18.0f };         // pixels
    constexpr float k_markerEdgeInset{ 14.0f };    // pixels from the viewport edge
    constexpr float k_maxExtraFlight{ 1.0f };      // seconds past travelTime before a bullet is removed

    // Distance along (dirX, dirZ) from `from`, which is inside the box, to the box edge.
    float DistanceToBoxEdge(const XMFLOAT3& from, float dirX, float dirZ,
        const XMFLOAT3& centre, float halfX, float halfZ)
    {
        float distance{ FLT_MAX };
        if (dirX > 1e-4f)  distance = min(distance, (centre.x + halfX - from.x) / dirX);
        if (dirX < -1e-4f) distance = min(distance, (centre.x - halfX - from.x) / dirX);
        if (dirZ > 1e-4f)  distance = min(distance, (centre.z + halfZ - from.z) / dirZ);
        if (dirZ < -1e-4f) distance = min(distance, (centre.z - halfZ - from.z) / dirZ);
        return max(distance, 0.0f);
    }
}

AttackConverge::AttackConverge(const ConvergeParams& params, Player* target)
    : m_params(params), m_target(target) {}

// Defined here so the unique_ptr can destroy Primitive, which the header only declares.
AttackConverge::~AttackConverge() = default;

void AttackConverge::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackConverge needs the phase's bullet pool");
    m_pool = pool;
    m_markerRenderer = std::make_unique<Primitive>(Graphics::Instance().GetDevice());

    m_hold.position = boss ? boss->GetPosition() : XMFLOAT3{};
    m_hold.lerpSpeed = 2.0f;

    m_pending.clear();
    m_incoming.clear();
    m_wavesQueued = 0;
    m_waveTimer = m_params.waveDelay; // the first wave's markers appear at once
    m_absorbedCount = 0;
    m_active = true;
}

void AttackConverge::Update(float dt, CueBoss* boss) {
    if (!m_active || !m_pool || !boss) return;

    const XMFLOAT3 bossPos{ boss->GetPosition() };

    // 1. Queue the next wave's markers.
    m_waveTimer += dt;
    if (m_wavesQueued < m_params.waves && m_waveTimer >= m_params.waveDelay) {
        m_waveTimer -= m_params.waveDelay;
        m_pending.push_back({ BuildWave(m_wavesQueued, bossPos), 0.0f });
        ++m_wavesQueued;
    }

    // 2. Launch waves whose warning is over.
    for (auto it = m_pending.begin(); it != m_pending.end(); ) {
        it->age += dt;
        if (it->age >= m_params.warningDuration) {
            LaunchWave(*it, bossPos);
            it = m_pending.erase(it);
            continue;
        }
        ++it;
    }

    // 3. Absorb arrivals; the attack ends once every wave has entered and landed.
    UpdateIncoming(dt, bossPos);
    if (m_wavesQueued >= m_params.waves && m_pending.empty() && m_incoming.empty()) {
        m_active = false;
    }
}

std::vector<XMFLOAT3> AttackConverge::BuildWave(int waveIndex, const XMFLOAT3& bossPos) const {
    std::vector<XMFLOAT3> points;
    if (m_params.bulletsPerWave <= 0) return points;

    const XMFLOAT3 viewCentre{ m_target ? m_target->GetPosition() : bossPos };
    const float halfX{ Beyond::Config::ARENA_WIDTH_UNITS * 0.5f + k_offscreenMargin };
    const float halfZ{ Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f + k_offscreenMargin };
    const float step{ XM_2PI / static_cast<float>(m_params.bulletsPerWave) };
    const float offset{ std::fmod(static_cast<float>(waveIndex) * k_goldenAngle, step) };

    points.reserve(static_cast<std::size_t>(m_params.bulletsPerWave));
    for (int i{ 0 }; i < m_params.bulletsPerWave; ++i) {
        const float angle{ offset + step * static_cast<float>(i) };
        const float dirX{ std::sin(angle) };
        const float dirZ{ std::cos(angle) };
        const float distance{ DistanceToBoxEdge(bossPos, dirX, dirZ, viewCentre, halfX, halfZ) };
        points.push_back({ bossPos.x + dirX * distance, bossPos.y, bossPos.z + dirZ * distance });
    }
    return points;
}

void AttackConverge::LaunchWave(const PendingWave& wave, const XMFLOAT3& bossPos) {
    for (const XMFLOAT3& point : wave.spawnPoints) {
        const ProjectileHandle handle{ m_pool->Spawn() };
        Projectile* projectile{ m_pool->Get(handle) };
        // Why: a full pool leaves a gap in the ring; the attack still ends on schedule.
        if (!projectile) continue;

        const float dx{ bossPos.x - point.x };
        const float dz{ bossPos.z - point.z };
        const float distance{ std::sqrt(dx * dx + dz * dz) };
        // Why speed from distance: every bullet of the wave reaches Cue at the same moment.
        const float speed{ (m_params.travelTime > 0.0f) ? distance / m_params.travelTime : distance };

        projectile->radius = m_params.bulletRadius;
        projectile->visualScale = m_params.visualScale;
        projectile->damage = m_params.damage;
        ProjectilePool::Launch(*projectile, point, { dx, 0.0f, dz }, speed);
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

void AttackConverge::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    if (!m_markerRenderer || !camera || m_pending.empty()) return;

    UINT viewportCount{ 1 };
    D3D11_VIEWPORT viewport{};
    context->RSGetViewports(&viewportCount, &viewport);

    const float minX{ k_markerEdgeInset };
    const float minY{ k_markerEdgeInset };
    const float maxX{ viewport.Width - k_markerEdgeInset - k_markerSize };
    const float maxY{ viewport.Height - k_markerEdgeInset - k_markerSize };

    for (const PendingWave& wave : m_pending) {
        // Blinks four times a second while the wave waits outside the view.
        const float blink{ (std::sin(wave.age * XM_2PI * 4.0f) + 1.0f) * 0.5f };
        const float alpha{ 0.35f + 0.65f * blink };
        for (const XMFLOAT3& point : wave.spawnPoints) {
            const XMFLOAT2 pixels{ WorldToViewportPixels(context, *camera, point) };
            const float x{ std::clamp(pixels.x - k_markerSize * 0.5f, minX, maxX) };
            const float y{ std::clamp(pixels.y - k_markerSize * 0.5f, minY, maxY) };
            m_markerRenderer->Rect(x, y, k_markerSize, k_markerSize, 0.0f, 0.0f, 0.0f,
                1.0f, 0.2f, 0.2f, alpha);
        }
    }
    m_markerRenderer->Render(context);
}

void AttackConverge::Stop(CueBoss* boss) {
    if (m_pool) {
        for (const IncomingBullet& bullet : m_incoming) {
            m_pool->Despawn(bullet.handle);
        }
    }
    m_incoming.clear();
    m_pending.clear();
    m_active = false;
}

bool AttackConverge::IsFinished() const {
    return !m_active;
}