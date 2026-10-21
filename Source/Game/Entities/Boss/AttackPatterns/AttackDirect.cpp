#pragma execution_character_set("utf-8")
#include "AttackDirect.h"
#include "Boss.h"
#include "ProjectilePool.h"
#include "Player.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

AttackDirect::AttackDirect(const DirectParams& params, Player* target)
    : m_params(params), m_target(target) {}

void AttackDirect::Start(Boss* boss, ProjectilePool* pool) {
    // Why: without the pool the attack spawns nothing and never reports a cause.
    assert(pool && "AttackDirect needs the phase's shared bullet pool");
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_spawnedCount = 0;
    m_currentTrigger = 0;
    m_isTriggerWaiting = false;
    m_triggerTimer = 0.0f;

    // Full, so the first bullet fires on the first Update.
    m_spawnTimer = m_params.spawnDelay;
}

void AttackDirect::Update(float dt, Boss* boss) {
    if (!m_active || !m_pool || !m_boss || !m_target) return;

    if (m_isTriggerWaiting) {
        m_triggerTimer += dt;
        if (m_triggerTimer >= m_params.triggerDelay) {
            m_isTriggerWaiting = false;
            m_spawnedCount = 0;
            m_spawnTimer = m_params.spawnDelay;
        }
        return;
    }

    m_spawnTimer += dt;
    if (m_spawnTimer >= m_params.spawnDelay) {
        m_spawnTimer -= m_params.spawnDelay;

        FireBullet();

        if (++m_spawnedCount >= m_params.count) {
            m_currentTrigger++;
            if (m_currentTrigger >= m_params.triggerCount) {
                m_active = false;
            }
            else {
                m_isTriggerWaiting = true;
                m_triggerTimer = 0.0f;
            }
        }
    }
}

void AttackDirect::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    // Bullets are rendered by the phase's central render loop
}

void AttackDirect::Stop(Boss* boss) {
    m_active = false;
}

bool AttackDirect::IsFinished() const {
    return !m_active;
}

void AttackDirect::FireBullet() {
    if (!m_pool || !m_boss || !m_target) return;

    if (Projectile* projectile{ m_pool->Get(m_pool->Spawn()) }) {
        // Aim at where the player is at the moment of firing.
        XMFLOAT3 bPos = m_boss->GetPosition();
        XMFLOAT3 pPos = m_target->GetPosition();

        float dx = pPos.x - bPos.x;
        float dz = pPos.z - bPos.z;
        float dist = sqrtf((dx * dx) + (dz * dz));

        XMFLOAT3 direction = { 0.0f, 0.0f, 1.0f }; // fallback when the player stands on the boss
        if (dist > 0.001f) {
            direction = { dx / dist, 0.0f, dz / dist };
        }

        projectile->radius = m_params.radius;
        projectile->visualScale = m_params.visualScale;
        projectile->damage = m_params.damage;
        ProjectilePool::Launch(*projectile, bPos, direction, m_params.speed);
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}