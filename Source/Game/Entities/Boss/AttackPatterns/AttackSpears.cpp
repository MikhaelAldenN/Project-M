#include "AttackSpears.h"
#include "CueBoss.h"
#include "WindowTrackingSystem.h"
#include "WindowManager.h"
#include "Player.h"
#include "System/Graphics.h"
#include "Engine/Common/Constants.h"
#include <algorithm>
#include <cassert>

using namespace DirectX;

namespace
{
    // Keeps a spear's 100-pixel window (2.5 units) fully on the monitor.
    constexpr float k_screenMargin{ 1.5f };
}

AttackSpears::AttackSpears(const UndyneSpearParams& params, Player* target)
    : m_params(params), m_playerTarget(target) {}

void AttackSpears::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackSpears needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    m_isSpawning = true;
    m_spawnedCount = 0;
    m_spawnTimer = 0.0f;
}

void AttackSpears::Update(float dt, CueBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // 1. Spawning around the player
    if (m_isSpawning) {
        m_spawnTimer -= dt;
        if (m_spawnTimer <= 0.0f) {
            m_spawnTimer = m_params.spawnDelay;

            // Why a fixed step instead of RNG: with the golden angle every spear lands far
            // from the previous one and the ring fills evenly, yet the order is the same every time.
            const float angleDeg{ static_cast<float>(m_spawnedCount) * m_params.angleStep };
            const float rad{ XMConvertToRadians(angleDeg) };

            const DirectX::XMFLOAT3 center{ m_playerTarget ? m_playerTarget->GetPosition() : DirectX::XMFLOAT3{} };
            const float limitX{ Beyond::Config::ARENA_WIDTH_UNITS * 0.5f - k_screenMargin };
            const float limitZ{ Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f - k_screenMargin };

            // Why clamped: Windowkill maps the visible area onto the monitor, and a spear's
            // window must not leave the screen.
            const DirectX::XMFLOAT3 spawnPos{
                std::clamp(center.x + cosf(rad) * m_params.ringRadius, -limitX, limitX),
                0.0f,
                std::clamp(center.z + sinf(rad) * m_params.ringRadius, -limitZ, limitZ)
            };

            UndyneSpearWindow spear;
            spear.projectile = m_pool->Spawn();
            // Why: a full pool leaves this spear out; the attack still ends on schedule.
            if (Projectile* projectile{ m_pool->Get(spear.projectile) }) {
                projectile->position = spawnPos;
                projectile->damage = m_params.damage;
                projectile->radius = 0.8f;
                // A hovering spear is shown but harmless until it launches.
                projectile->hitsPlayer = false;

                spear.windowName = "Spear_" + std::to_string(rand() % 100000) + "_" + std::to_string(m_spawnedCount);

                TrackedWindowConfig config = { spear.windowName, "Spear.exe", 100, 100, 10 };
                boss->GetWindowSystem()->AddTrackedWindow(config, [pool = m_pool, index = spear.projectile.index]() {
                    // Why the slot and not Get: a collision can despawn the projectile before the
                    // window updates; the window then keeps its last position until this attack
                    // removes the window on its next Update.
                    return pool->Slots()[index].position;
                });

                m_spears.push_back(std::move(spear));
            }

            if (++m_spawnedCount >= m_params.count) {
                m_isSpawning = false;
            }
        }
    }

    // 2. Spear Hover & Homing Update
    for (auto& spear : m_spears) {
        if (spear.isPreparedForDestroy) continue;

        Projectile* projectile{ m_pool->Get(spear.projectile) };
        // A spear that hit the player leaves the same way as one that flew off the arena.
        if (!projectile) {
            RetireSpear(spear, boss);
            continue;
        }

        spear.timer += dt;
        DirectX::XMFLOAT3 bPos = projectile->position;

        if (spear.state == 0 && m_playerTarget) {
            DirectX::XMFLOAT3 pPos = m_playerTarget->GetPosition();
            float dx = pPos.x - bPos.x;
            float dz = pPos.z - bPos.z;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist > 0.001f) {
                spear.lockDir = { dx / dist, 0.0f, dz / dist };
                projectile->yawDegrees = XMConvertToDegrees(atan2f(dx, dz));
            }

            if (spear.timer >= m_params.hoverDuration) {
                spear.state = 2; // Launch!
                spear.timer = 0.0f;
                spear.currentSpeed = m_params.startSpeed;
                projectile->hitsPlayer = true;
            }
        }
        else if (spear.state == 2) {
            spear.currentSpeed += m_params.acceleration * dt;
            if (spear.currentSpeed > m_params.maxSpeed) spear.currentSpeed = m_params.maxSpeed;

            projectile->velocity = {
                spear.lockDir.x * spear.currentSpeed,
                0.0f,
                spear.lockDir.z * spear.currentSpeed
            };
            ProjectilePool::Advance(*projectile, dt);

            if (bPos.x < -40.0f || bPos.x > 40.0f || bPos.z < -40.0f || bPos.z > 40.0f) {
                RetireSpear(spear, boss);
            }
        }
    }
}

void AttackSpears::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    if (!m_pool || !m_mesh.IsLoaded()) return;
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    for (const auto& spear : m_spears) {
        if (spear.isPreparedForDestroy) continue;
        if (const Projectile* projectile{ m_pool->Get(spear.projectile) }) {
            m_mesh.Draw(*modelRenderer, ShaderId::Phong, { 0.0f, 0.8f, 1.0f, 1.0f },
                projectile->position, projectile->visualScale, XMConvertToRadians(projectile->yawDegrees));
        }
    }
}

void AttackSpears::Stop(CueBoss* boss) {
    if (boss && boss->GetWindowSystem()) {
        for (auto& spear : m_spears) {
            if (!spear.isPreparedForDestroy) {
                boss->GetWindowSystem()->RemoveTrackedWindow(spear.windowName);
            }
        }
    }
    if (m_pool) {
        for (const auto& spear : m_spears) m_pool->Despawn(spear.projectile);
    }
    m_spears.clear();
    m_isSpawning = false;
}

bool AttackSpears::IsFinished() const {
    if (m_isSpawning) return false;
    for (const auto& spear : m_spears) {
        if (!spear.isPreparedForDestroy) return false;
    }
    return true;
}

void AttackSpears::RetireSpear(UndyneSpearWindow& spear, CueBoss* boss) {
    spear.isPreparedForDestroy = true;
    m_pool->Despawn(spear.projectile);

    auto extracted = boss->GetWindowSystem()->ExtractForPool(spear.windowName);
    if (extracted && extracted->window) {
        WindowManager::Instance().DestroyWindow(extracted->window);
    }
}