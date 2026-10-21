#include "AttackBoomerangs.h"
#include "Boss.h"
#include "WindowTrackingSystem.h"
#include "CameraController.h"
#include <SDL3/SDL.h>
#include <cassert>

using namespace DirectX;

AttackBoomerangs::AttackBoomerangs(const BoomerangParams& params)
    : m_params(params) {}

void AttackBoomerangs::Start(Boss* boss, ProjectilePool* pool) {
    assert(pool && "AttackBoomerangs needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    m_isSpawning = true;
    m_spawnedCount = 0;
    m_spawnTimer = m_params.spawnDelay;
}

void AttackBoomerangs::Update(float dt, Boss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // Edges of the arena in world units, the same on every monitor.
    const float limitX{ Beyond::Config::ARENA_WIDTH_UNITS * 0.5f };
    const float limitZ{ Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f };

    // 1. Spawning Sequence
    if (m_isSpawning) {
        m_spawnTimer += dt;
        while (m_isSpawning && m_spawnTimer >= m_params.spawnDelay) {
            m_spawnTimer = (m_params.spawnDelay > 0.0f) ? m_spawnTimer - m_params.spawnDelay : 1.0f;

            BoomerangWindowBullet bw;
            bw.projectile = m_pool->Spawn();
            m_spawnedCount++;
            if (m_spawnedCount >= m_params.spawnCount) m_isSpawning = false;

            Projectile* projectile{ m_pool->Get(bw.projectile) };
            // Why: a full pool leaves this boomerang out; the attack still ends on schedule.
            if (!projectile) continue;
            projectile->radius = m_params.hitboxRadius;
            projectile->damage = m_params.damage;
            projectile->visualScale = m_params.visualScale;

            bw.spawnSide = (rand() % 2 == 0) ? 1 : -1;
            bw.startX = bw.spawnSide * (limitX + 8.0f);
            bw.targetX = bw.startX - (bw.spawnSide * m_params.maxTravelDistance);

            float randomZ = m_params.spawnBottomHalfOnly
                ? -((rand() % 100) / 100.0f) * (limitZ - 3.0f)
                : ((rand() % 200) / 100.0f - 1.0f) * (limitZ - 3.0f);

            DirectX::XMFLOAT3 startPos = { bw.startX, 1.0f, randomZ };
            bw.targetVelX = -1.0f * bw.spawnSide * m_params.speed;

            DirectX::XMFLOAT3 startDir = { bw.targetVelX > 0 ? 1.0f : -1.0f, 0.0f, 0.0f };
            ProjectilePool::Launch(*projectile, startPos, startDir, m_params.speed);
            bw.windowName = "boomerang_win_" + std::to_string(rand() % 100000);

            TrackedWindowConfig cfg = { bw.windowName, "BOOMERANG WARNING", (int)m_params.windowSize, (int)m_params.windowSize, 8 };
            cfg.isTransparent = false;
            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [pool = m_pool, index = bw.projectile.index]() {
                    // Why the slot and not Get: a collision can despawn the projectile before the
                    // window updates; the window then keeps its last position until this attack
                    // removes the window on its next Update.
                    return pool->Slots()[index].position;
                },
                [this]() { return DirectX::XMFLOAT2(m_params.windowSize, m_params.windowSize); }
            );

            m_bullets.push_back(std::move(bw));
            CameraController::Instance().AddTrauma(0.1f);
        }
    }

    // 2. Process Return Logic
    for (auto it = m_bullets.begin(); it != m_bullets.end(); ) {
        auto& bw = *it;
        Projectile* projectile{ m_pool->Get(bw.projectile) };
        if (!projectile) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
            it = m_bullets.erase(it);
            continue;
        }

        DirectX::XMFLOAT3 vel = projectile->velocity;
        // Smooth Velocity Lerp
        vel.x += (bw.targetVelX - vel.x) * m_params.turnSpeed * dt;

        projectile->velocity = vel;
        ProjectilePool::Advance(*projectile, dt);

        DirectX::XMFLOAT3 pos = projectile->position;

        if (bw.state == 0) { // MODE MASUK
            if ((bw.spawnSide == 1 && pos.x <= bw.targetX) || (bw.spawnSide == -1 && pos.x >= bw.targetX)) {
                bw.state = 1;
                bw.targetVelX = bw.spawnSide * m_params.speed; // Switch direction
            }
        }
        else if (bw.state == 1) { // MODE KELUAR
            if ((bw.spawnSide == 1 && pos.x >= bw.startX) || (bw.spawnSide == -1 && pos.x <= bw.startX)) {
                boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
                m_pool->Despawn(bw.projectile);
                it = m_bullets.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void AttackBoomerangs::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    if (!m_pool || !m_mesh.IsLoaded()) return;
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    for (const auto& bw : m_bullets) {
        if (const Projectile* projectile{ m_pool->Get(bw.projectile) }) {
            m_mesh.Draw(*modelRenderer, ShaderId::Phong, { 0.6f, 0.0f, 0.8f, 1.0f },
                projectile->position, projectile->visualScale, 0.0f);
        }
    }
}

void AttackBoomerangs::Stop(Boss* boss) {
    if (boss && boss->GetWindowSystem()) {
        for (auto& bw : m_bullets) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bw.windowName);
        }
    }
    if (m_pool) {
        for (const auto& bw : m_bullets) m_pool->Despawn(bw.projectile);
    }
    m_bullets.clear();
    m_isSpawning = false;
}

bool AttackBoomerangs::IsFinished() const {
    return !m_isSpawning && m_bullets.empty();
}