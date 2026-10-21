#pragma execution_character_set("utf-8")
#include "AttackMeteor.h"
#include "Boss.h"
#include "ProjectilePool.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>
#include <cmath>

using namespace DirectX;

AttackMeteor::AttackMeteor(const MeteorParams& params)
    : m_params(params) {}

void AttackMeteor::Start(Boss* boss, ProjectilePool* pool) {
    assert(pool && "AttackMeteor needs the phase's bullet pool");
    m_pool = pool;
    m_active = true;
    m_spawnedCount = 0;
    m_currentTrigger = 0;
    m_isTriggerWaiting = false;
    m_triggerTimer = 0.0f;

    // Set agar meteor pertama langsung meluncur
    m_spawnTimer = m_params.spawnDelay;
}

void AttackMeteor::Update(float dt, Boss* boss) {
    if (!m_active || !m_pool) return;

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

        FireMeteor();

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

void AttackMeteor::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    // Peluru dirender terpusat oleh Phase
}

void AttackMeteor::Stop(Boss* boss) {
    m_active = false;
}

bool AttackMeteor::IsFinished() const {
    return !m_active;
}

void AttackMeteor::FireMeteor() {
    if (!m_pool) return;

    if (Projectile* meteor{ m_pool->Get(m_pool->Spawn()) }) {
        // 1. Main direction of travel, from the start anchor to the target anchor.
        float dx = m_params.targetX - m_params.startX;
        float dz = m_params.targetZ - m_params.startZ;
        float dist = sqrtf((dx * dx) + (dz * dz));

        float dirX = 0.0f;
        float dirZ = 1.0f; // fallback when both anchors coincide
        if (dist > 0.0001f) {
            dirX = dx / dist;
            dirZ = dz / dist;
        }

        // 2. Perpendicular to the travel direction: the line the meteors spread along.
        float perpX = -dirZ;
        float perpZ = dirX;

        // A prime jump scatters the spawn order along that line (count 5: 0, 2, 4, 1, 3).
        int primeJump = 5;
        int scatterIndex = (m_spawnedCount * primeJump) % m_params.count;

        float centerIndex = (m_params.count - 1) / 2.0f;
        float currentOffset = (scatterIndex - centerIndex) * m_params.spreadOffset;

        // Random X jitter in [-8, 8).
        float randomOffsetX = ((rand() % 100) / 50.0f - 1.0f) * 8.0f;

        DirectX::XMFLOAT3 spawnPos = {
            m_params.startX + (perpX * currentOffset) + randomOffsetX,
            1.0f,
            m_params.startZ + (perpZ * currentOffset)
        };
        DirectX::XMFLOAT3 direction = { dirX, 0.0f, dirZ };

        // Random speed offset in [-speedVariance, speedVariance), never below 5.
        float randomSpeedOffset = ((rand() % 100) / 50.0f - 1.0f) * m_params.speedVariance;
        float finalSpeed = m_params.speed + randomSpeedOffset;
        if (finalSpeed < 5.0f) finalSpeed = 5.0f;

        meteor->radius = m_params.radius;
        meteor->visualScale = m_params.visualScale;
        meteor->damage = m_params.damage;
        ProjectilePool::Launch(*meteor, spawnPos, direction, finalSpeed);
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}