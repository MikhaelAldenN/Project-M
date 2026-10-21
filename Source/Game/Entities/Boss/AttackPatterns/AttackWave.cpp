#pragma execution_character_set("utf-8")
#include "AttackWave.h"
#include "Boss.h"
#include "ProjectilePool.h"
#include "System/AudioManager.h"
#include <DirectXMath.h>

using namespace DirectX;

AttackWave::AttackWave(const WaveParams& params)
    : m_params(params) {}

void AttackWave::Start(Boss* boss, ProjectilePool* pool) {
    assert(pool && "AttackWave needs the phase's bullet pool");
    m_pool = pool;
    m_boss = boss;
    m_active = true;
    m_wavesFired = 0;
    m_waveTimer = m_params.waveDelay; // Set penuh agar langsung nembak di frame pertama
    m_nextIsRight = true;             // Gelombang pertama dari kanan
}

void AttackWave::Update(float dt, Boss* boss) {
    if (!m_active || !m_pool) return;

    m_waveTimer += dt;
    if (m_waveTimer >= m_params.waveDelay) {
        m_waveTimer -= m_params.waveDelay;

        FireWave(m_nextIsRight);

        m_nextIsRight = !m_nextIsRight; // Balikkan arah untuk gelombang berikutnya

        if (++m_wavesFired >= m_params.waves) {
            m_active = false;
        }
    }
}

void AttackWave::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    // Peluru di-render oleh BossPhase01 secara terpusat
}

void AttackWave::Stop(Boss* boss) {
    m_active = false;
}

bool AttackWave::IsFinished() const {
    return !m_active;
}

// ============================================================
// Internal Logic
// ============================================================
void AttackWave::FireWave(bool fromRight) {
    if (!m_pool) return;

    // Right uses the odd tracks (1, 3, 5...), left the even ones (0, 2, 4...).
    int startIndex = fromRight ? 1 : 0;

    for (int fired = 0; fired < m_params.bulletsPerWave; ++fired) {
        int trackIndex = startIndex + (fired * 2);
        if (trackIndex >= m_params.trackCount) break;

        Projectile* projectile{ m_pool->Get(m_pool->Spawn()) };
        if (!projectile) break; // pool is full

        float zPos = m_params.startZ + (trackIndex * m_params.trackSpacing);
        float xPos = fromRight ? m_params.spawnX : -m_params.spawnX;

        DirectX::XMFLOAT3 spawnPos = { xPos, 1.0f, zPos };
        DirectX::XMFLOAT3 direction = { fromRight ? -1.0f : 1.0f, 0.0f, 0.0f };

        projectile->radius = 0.35f;
        projectile->visualScale = 1.5f;
        projectile->damage = m_params.damage;
        ProjectilePool::Launch(*projectile, spawnPos, direction, m_params.speed);
    }

    AudioManager::Instance().PlaySFX("Data/Sound/SE_Boss_Shoot.wav", 0.2f * m_params.sfxVolume);
}