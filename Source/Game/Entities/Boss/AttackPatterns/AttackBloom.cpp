#include "AttackBloom.h"
#include "CueBoss.h"
#include "System/AudioManager.h"
#include <cassert>
#include <cmath>
#include <utility>

using namespace DirectX;

namespace
{
    constexpr float k_pulseAmount{ 0.25f };   // USULAN DESAIN: visual scale swings +-25% while holding
    constexpr float k_pulsePerSecond{ 4.0f };

    // USULAN DESAIN: cubic ease-out, fast off Cue and slowing to rest at the stop radius.
    float EaseOutCubic(float t)
    {
        const float remaining{ 1.0f - t };
        return 1.0f - (remaining * remaining * remaining);
    }
}

AttackBloom::AttackBloom(const BloomParams& params)
    : m_params(params) {}

void AttackBloom::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackBloom needs the phase's bullet pool");
    m_pool = pool;
    m_rings.clear();
    m_ringsEmitted = 0;
    m_ringTimer = m_params.ringDelay; // the first ring leaves at once
    m_active = true;
}

void AttackBloom::Update(float dt, CueBoss* boss) {
    if (!m_active || !m_pool || !boss) return;

    m_ringTimer += dt;
    while (m_ringsEmitted < m_params.ringCount && m_ringTimer >= m_params.ringDelay) {
        m_ringTimer -= m_params.ringDelay;
        EmitRing(m_ringsEmitted, boss->GetPosition());
        ++m_ringsEmitted;
    }

    const XMFLOAT3 bossPos{ boss->GetPosition() };
    for (auto it = m_rings.begin(); it != m_rings.end(); ) {
        if (!UpdateRing(*it, dt, bossPos)) {
            it = m_rings.erase(it);
            continue;
        }
        ++it;
    }

    // Launched bullets fly on under the pool; the attack ends with the last launch.
    if (m_ringsEmitted >= m_params.ringCount && m_rings.empty()) {
        m_active = false;
    }
}

void AttackBloom::EmitRing(int ringIndex, const XMFLOAT3& centre) {
    if (m_params.bulletsPerRing <= 0) return;

    Ring ring{};
    const float step{ XM_2PI / static_cast<float>(m_params.bulletsPerRing) };
    // Every other ring is turned half a step, so its bullets fill the previous ring's gaps.
    const float offset{ (ringIndex % 2 == 1) ? step * 0.5f : 0.0f };

    for (int i{ 0 }; i < m_params.bulletsPerRing; ++i) {
        const ProjectileHandle handle{ m_pool->Spawn() };
        Projectile* projectile{ m_pool->Get(handle) };
        // Why: a full pool leaves a gap in the ring; the attack still ends on schedule.
        if (!projectile) continue;

        const float angle{ offset + step * static_cast<float>(i) };
        const XMFLOAT2 direction{ std::sin(angle), std::cos(angle) };
        projectile->radius = m_params.bulletRadius;
        projectile->visualScale = m_params.visualScale;
        projectile->damage = m_params.damage;
        // Why no velocity: UpdateRing places ring bullets itself until they launch.
        projectile->position = centre;
        projectile->velocity = { 0.0f, 0.0f, 0.0f };

        ring.bullets.push_back(handle);
        ring.directions.push_back(direction);
    }

    m_rings.push_back(std::move(ring));
    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f);
}

bool AttackBloom::UpdateRing(Ring& ring, float dt, const XMFLOAT3& bossPos) {
    ring.age += dt;
    // The out leg keeps its tuned length (stopRadius / outSpeed); the ease only shapes it.
    const float stopTime{ (m_params.outSpeed > 0.0f) ? m_params.stopRadius / m_params.outSpeed : 0.0f };
    const float launchTime{ stopTime + m_params.holdDuration };
    const bool isLaunching{ ring.age >= launchTime };
    const bool isHolding{ !isLaunching && ring.age >= stopTime };

    // Until it launches the ring is centred on Cue and moves with her, the way
    // Phalanx bullets orbit her.
    const XMFLOAT3& centre{ bossPos };

    const bool isGoingOut{ !isHolding && !isLaunching && stopTime > 0.0f };
    const float radius{ isGoingOut ? m_params.stopRadius * EaseOutCubic(ring.age / stopTime) : m_params.stopRadius };
    const float pulse{ isHolding ? 1.0f + k_pulseAmount * std::sin(ring.age * XM_2PI * k_pulsePerSecond) : 1.0f };

    for (std::size_t i{ 0 }; i < ring.bullets.size(); ++i) {
        Projectile* projectile{ m_pool->Get(ring.bullets[i]) };
        // Gone already: it hit the player.
        if (!projectile) continue;

        const XMFLOAT2& direction{ ring.directions[i] };
        const XMFLOAT3 point{ centre.x + direction.x * radius, centre.y, centre.z + direction.y * radius };

        if (isLaunching) {
            projectile->visualScale = m_params.visualScale;
            ProjectilePool::Launch(*projectile, point, { direction.x, 0.0f, direction.y }, m_params.launchSpeed);
        }
        else {
            // Why placed, not moved: the ring stays round whatever the frame rate.
            projectile->position = point;
            projectile->velocity = { 0.0f, 0.0f, 0.0f };
            projectile->visualScale = m_params.visualScale * pulse;
        }
    }

    if (isLaunching) {
        AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f);
        return false;
    }
    return true;
}

void AttackBloom::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    // Bullets are rendered by the phase's central render loop
}

void AttackBloom::Stop(CueBoss* boss) {
    if (m_pool) {
        for (const Ring& ring : m_rings) {
            for (const ProjectileHandle handle : ring.bullets) {
                m_pool->Despawn(handle);
            }
        }
    }
    m_rings.clear();
    m_active = false;
}

bool AttackBloom::IsFinished() const {
    return !m_active;
}
