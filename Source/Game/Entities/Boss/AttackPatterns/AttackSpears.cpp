#include "AttackSpears.h"
#include "Boss.h"
#include "WindowTrackingSystem.h"
#include "WindowManager.h"
#include "Player.h"
#include "System/Graphics.h"
#include <algorithm>
#include <cassert>
#include <random>

using namespace DirectX;

AttackSpears::AttackSpears(const UndyneSpearParams& params, Player* target)
    : m_params(params), m_playerTarget(target) {}

void AttackSpears::Start(Boss* boss, ProjectilePool* pool) {
    assert(pool && "AttackSpears needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    m_isSpawning = true;
    m_spawnedCount = 0;
    m_spawnTimer = 0.0f;

    // Prepare randomized spawn indices
    m_spawnIndices.clear();
    for (int i = 0; i < m_params.count; ++i) {
        m_spawnIndices.push_back(i);
    }
    static std::mt19937 gen(std::random_device{}());
    std::shuffle(m_spawnIndices.begin(), m_spawnIndices.end(), gen);
}

void AttackSpears::Update(float dt, Boss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // 1. Spawning Rainbow Arc
    if (m_isSpawning) {
        m_spawnTimer -= dt;
        if (m_spawnTimer <= 0.0f) {
            m_spawnTimer = m_params.spawnDelay;

            float angleRange = m_params.arcMaxAngle - m_params.arcMinAngle;
            float step = (m_params.count > 1) ? (angleRange / (m_params.count - 1)) : 0.0f;

            int randomPositionIndex = m_spawnIndices[m_spawnedCount];
            float angleDeg = m_params.arcMinAngle + (randomPositionIndex * step);
            float rad = XMConvertToRadians(angleDeg);

            DirectX::XMFLOAT3 spawnPos = {
                m_params.arcCenterX + cosf(rad) * m_params.arcRadius,
                0.0f,
                m_params.arcCenterZ + sinf(rad) * m_params.arcRadius
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

void AttackSpears::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
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

void AttackSpears::Stop(Boss* boss) {
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

void AttackSpears::RetireSpear(UndyneSpearWindow& spear, Boss* boss) {
    spear.isPreparedForDestroy = true;
    m_pool->Despawn(spear.projectile);

    auto extracted = boss->GetWindowSystem()->ExtractForPool(spear.windowName);
    if (extracted && extracted->window) {
        WindowManager::Instance().DestroyWindow(extracted->window);
    }
}