#include "AttackBouncing.h"
#include "Boss.h"
#include "WindowTrackingSystem.h"
#include "System/AudioManager.h"
#include "CameraController.h"
#include <SDL3/SDL.h>
#include <cassert>

using namespace DirectX;

AttackBouncing::AttackBouncing(const BouncingBulletParams& params)
    : m_params(params) {}

void AttackBouncing::Start(Boss* boss, BossBulletPool* pool) {
    assert(pool && "AttackBouncing needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    m_isSpawning = true;
    m_spawnedCount = 0;
    m_spawnTimer = m_params.spawnDelay; // Force immediate first spawn

    // Cache screen limits based on pixel-to-unit ratio
    m_screenLimitX = Beyond::Config::ARENA_WIDTH_UNITS * 0.5f;
    m_screenLimitZ = Beyond::Config::ARENA_HEIGHT_UNITS * 0.5f;
}

void AttackBouncing::Update(float dt, Boss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    // 1. Handle Spawning Sequence
    if (m_isSpawning) {
        m_spawnTimer += dt;
        while (m_isSpawning && m_spawnTimer >= m_params.spawnDelay) {
            m_spawnTimer = (m_params.spawnDelay > 0.0f) ? m_spawnTimer - m_params.spawnDelay : 1.0f;

            // Asymmetric pattern: Right, Top-Right, Top-Left
            float patternAngles[3] = {
                XMConvertToRadians(35.0f),
                XMConvertToRadians(75.0f),
                XMConvertToRadians(145.0f)
            };

            float angle = patternAngles[m_spawnedCount % 3];
            if (m_spawnedCount >= 3) {
                angle += XMConvertToRadians((float)((rand() % 10) - 5)); // Add chaos
            }

            BouncingWindowBullet bwb;
            bwb.projectile = m_pool->Spawn();
            m_spawnedCount++;
            if (m_spawnedCount >= m_params.spawnCount) m_isSpawning = false;

            Projectile* projectile{ m_pool->Get(bwb.projectile) };
            // Why: a full pool leaves this bullet out; the attack still ends on schedule.
            if (!projectile) continue;
            projectile->radius = m_params.hitboxRadius;
            projectile->damage = m_params.damage;
            projectile->visualScale = m_params.visualScale;
            bwb.maxBounces = m_params.maxBounces;

            DirectX::XMFLOAT3 dir = { cosf(angle), 0, sinf(angle) };
            ProjectilePool::Launch(*projectile, boss->GetPosition(), dir, m_params.speed);
            bwb.windowName = "bouncing_win_" + std::to_string(rand() % 100000);

            // Register Window tracking
            TrackedWindowConfig cfg;
            cfg.name = bwb.windowName;
            cfg.title = "PROJECTILE";
            cfg.width = (int)m_params.windowWidth;
            cfg.height = (int)m_params.windowHeight;
            cfg.isTransparent = false;

            boss->GetWindowSystem()->AddTrackedWindow(cfg,
                [pool = m_pool, index = bwb.projectile.index]() {
                    // Why the slot and not Get: a collision can despawn the projectile before the
                    // window updates; the window then keeps its last position until this attack
                    // removes the window on its next Update.
                    return pool->Slots()[index].position;
                },
                [this]() { return DirectX::XMFLOAT2(m_params.windowWidth, m_params.windowHeight); }
            );

            m_bullets.push_back(std::move(bwb));

            CameraController::Instance().AddTrauma(0.2f);
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Bouncing_Shoot.wav", 0.1f);
        }
    }

    // 2. Process Physics & Screen Bouncing
    bool anyBouncedThisFrame = false;
    for (auto it = m_bullets.begin(); it != m_bullets.end(); ) {
        auto& bwb = *it;

        Projectile* projectile{ m_pool->Get(bwb.projectile) };
        if (!projectile) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
            it = m_bullets.erase(it);
            continue;
        }

        ProjectilePool::Advance(*projectile, dt);

        DirectX::XMFLOAT3 pos = projectile->position;
        DirectX::XMFLOAT3 vel = projectile->velocity;
        float radius = projectile->radius;

        if (bwb.bounceCount < bwb.maxBounces) {
            bool bounced = false;
            // X-Axis bounds
            if (pos.x > m_screenLimitX - radius) { pos.x = m_screenLimitX - radius; vel.x *= -1.0f; bounced = true; }
            else if (pos.x < -m_screenLimitX + radius) { pos.x = -m_screenLimitX + radius; vel.x *= -1.0f; bounced = true; }

            // Z-Axis bounds
            if (pos.z > m_screenLimitZ - radius) { pos.z = m_screenLimitZ - radius; vel.z *= -1.0f; bounced = true; }
            else if (pos.z < -m_screenLimitZ + radius) { pos.z = -m_screenLimitZ + radius; vel.z *= -1.0f; bounced = true; }

            if (bounced) {
                bwb.bounceCount++;
                projectile->position = pos;
                projectile->velocity = vel;
                anyBouncedThisFrame = true;
            }
        }
        else {
            // Destroy if out of bounds after max bounces
            if (abs(pos.x) > m_screenLimitX + 15.0f || abs(pos.z) > m_screenLimitZ + 15.0f) {
                boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
                m_pool->Despawn(bwb.projectile);
                it = m_bullets.erase(it);
                continue;
            }
        }
        ++it;
    }

    if (anyBouncedThisFrame) {
        CameraController::Instance().AddTrauma(0.3f);
        std::string dashSounds[] = {
            "Data/Sound/SE_Boss_Bouncing_Thud_01.wav",
            "Data/Sound/SE_Boss_Bouncing_Thud_02.wav",
            "Data/Sound/SE_Boss_Bouncing_Thud_03.wav"
        };
        AudioManager::Instance().PlaySFX(dashSounds[rand() % 3], 0.2f);
    }
}

void AttackBouncing::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    if (!m_pool || !m_mesh.IsLoaded()) return;
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    for (const auto& bwb : m_bullets) {
        if (const Projectile* projectile{ m_pool->Get(bwb.projectile) }) {
            m_mesh.Draw(*modelRenderer, ShaderId::Phong, { 1.0f, 0.4f, 0.0f, 1.0f },
                projectile->position, projectile->visualScale, 0.0f);
        }
    }
}

void AttackBouncing::Stop(Boss* boss) {
    if (boss && boss->GetWindowSystem()) {
        for (auto& bwb : m_bullets) {
            boss->GetWindowSystem()->RemoveTrackedWindow(bwb.windowName);
        }
    }
    if (m_pool) {
        for (const auto& bwb : m_bullets) m_pool->Despawn(bwb.projectile);
    }
    m_bullets.clear();
    m_isSpawning = false;
}

bool AttackBouncing::IsFinished() const {
    return !m_isSpawning && m_bullets.empty();
}