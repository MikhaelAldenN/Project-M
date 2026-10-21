#pragma once
#include "IBossAttackPattern.h"
#include <DirectXMath.h>
#include <vector>

// ============================================================
// AttackWave - Phase 1 attack pattern.
//
// Fires straight lines of bullets from off screen, alternating sides:
// a wave from the right fills the odd tracks, the next wave from the
// left fills the even tracks.
// ============================================================

struct WaveParams {
    int   trackCount = 10;      // number of tracks
    int   bulletsPerWave = 5;   // bullets per wave
    int   waves = 6;            // waves in total, sides alternating
    float waveDelay = 0.8f;     // seconds between waves
    float speed = 25.0f;
    float trackSpacing = 3.5f;  // Z distance between tracks
    float startZ = -15.75f;     // Z of track 0, the bottom track
    float spawnX = 40.0f;       // spawn distance from the arena centre, off screen
    int   damage = 2;
    float sfxVolume = 1.0f;
    DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 0.0f, 1.0f }; // yellow
};

class AttackWave : public IBossAttackPattern {
public:
    explicit AttackWave(const WaveParams& params);
    ~AttackWave() override = default;

    void Start(Boss* boss, ProjectilePool* pool) override;
    void Update(float dt, Boss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) override;
    void Stop(Boss* boss) override;

    bool IsFinished() const override;

private:
    void FireWave(bool fromRight);

    WaveParams                            m_params;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    Boss* m_boss = nullptr;

    bool  m_active = false;
    int   m_wavesFired = 0;
    float m_waveTimer = 0.0f;
    bool  m_nextIsRight = true; // alternates every wave
};