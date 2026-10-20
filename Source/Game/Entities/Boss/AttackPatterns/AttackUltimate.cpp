#pragma execution_character_set("utf-8")
#include "AttackUltimate.h"
#include "Boss.h"
#include "ProjectilePool.h"
#include "Player.h"
#include "System/Graphics.h"
#include "System/AudioManager.h"
#include "CameraController.h"
#include "EffectManager.h"
#include <DirectXMath.h>
#include <random>
#include <cmath>

using namespace DirectX;

AttackUltimate::AttackUltimate(const UltimateParams& params, Player* target)
    : m_params(params), m_target(target) {}

void AttackUltimate::Start(Boss* boss, BossBulletPool* pool) {
    assert(pool && "AttackUltimate needs the phase's bullet pool");
    m_pool = pool;
    m_state = State::Moving;
    m_chargeTimer = 0.0f;
    m_ball = ProjectileHandle{};
    m_chargeEffectHandle = -1;

    // Boss slides to top center to charge
    m_moveTarget.position = { 0.0f, 0.0f, 10.0f };
    m_moveTarget.lerpSpeed = m_params.attackMoveSpeed;

    ClaimBall();
}

void AttackUltimate::Update(float dt, Boss* boss) {
    if (m_state == State::Done || !m_pool || !boss) return;

    XMFLOAT3 bPos = boss->GetPosition();

    // --- Moving: wait for boss to reach charge position ---
    if (m_state == State::Moving) {
        float dx = m_moveTarget.position.x - bPos.x;
        float dz = m_moveTarget.position.z - bPos.z;
        if ((dx * dx + dz * dz) <= 1.0f) {
            m_state = State::Charging;
            m_chargeTimer = 0.0f;
            // The ball is visible from the start of the charge.
            if (!m_pool->Get(m_ball)) ClaimBall();
        }
        else {
            // Never deactivate the ball here.
            return;
        }
    }

    // --- Charging ---
    if (m_state == State::Charging) {
        if (m_chargeTimer == 0.0f) {
            AudioManager::Instance().PlaySFX(
                "Data/Sound/SE_Boss_Bijuudama_Charge.wav", 0.1f * m_params.sfxVolume);
        }

        // A ball lost to a collision is claimed again, so the charge always shows one.
        if (!m_pool->Get(m_ball)) ClaimBall();
        Projectile* ball{ m_pool->Get(m_ball) };

        // Spawn charge VFX once
        if (m_chargeEffectHandle == -1 && ball) {
            XMFLOAT3 spawnPos = ball->position;
            m_chargeEffectHandle = EffectManager::Instance().Play(
                "Data/Effect/VFX_Boss_Bijuudama_Charge.efk", spawnPos, 1.0f);
            m_pool->AttachVfx(m_ball.index, "Data/Effect/VFX_Boss_Fireball.efk", ball->visualScale * 0.3f);
        }

        m_chargeTimer += dt;

        // Position and grow ball while stationary
        if (ball) {
            XMFLOAT3 vel = ball->velocity;
            if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {
                XMFLOAT3 offsetPos = bPos;
                offsetPos.z -= m_params.spawnOffsetZ;
                ball->position = offsetPos;

                float progress = min(1.0f, m_chargeTimer / m_params.laserDuration);
                float currentHitbox = m_params.baseHitbox + (m_params.maxHitboxGrow * progress);
                ball->radius = currentHitbox;
                float visualScale = currentHitbox * m_params.visualMultiplier;
                ball->visualScale = visualScale;

                // Sync charge VFX scale to ball
                if (m_chargeEffectHandle != -1 &&
                    EffectManager::Instance().IsPlaying(m_chargeEffectHandle))
                {
                    EffectManager::Instance().SetPosition(m_chargeEffectHandle, offsetPos);
                    float vfxScale = visualScale * 0.5f;
                    EffectManager::Instance().SetScale(
                        m_chargeEffectHandle, { vfxScale, vfxScale, vfxScale });
                }
            }
        }

        if (m_chargeTimer >= m_params.laserDuration) {
            LaunchBall(boss);
        }
    }

    // --- Recovering ---
    if (m_state == State::Recovering) {
        m_recoveryTimer += dt;
        if (m_recoveryTimer >= m_params.postFireDelay) {
            m_state = State::Done;
            m_moveTarget.position = { 0.0f, 0.0f, 0.0f };
            m_moveTarget.lerpSpeed = m_params.returnMoveSpeed;
        }
    }
}

void AttackUltimate::Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) {
    if (m_state != State::Charging || !m_target) return;

    auto shapeRenderer = Graphics::Instance().GetShapeRenderer();
    XMFLOAT3 pPos = m_target->GetPosition();
    pPos.y += 1.0f;

    float t = min(1.0f, m_chargeTimer / m_params.laserDuration);
    float ringRadius = m_params.laserStartRadius +
        (m_params.laserTargetRadius - m_params.laserStartRadius) * t;

    XMFLOAT4 ringColor = { 1.0f, 0.0f, 0.0f, 1.0f };
    if (IsInParryWindow())
        ringColor = { 1.0f, 1.0f, 1.0f, 1.0f };

    shapeRenderer->DrawSphere(pPos, ringRadius, ringColor);
}

void AttackUltimate::Stop(Boss* boss) {
    CancelCharge();
    if (m_pool) m_pool->Despawn(m_ball);
    m_ball = ProjectileHandle{};
    m_state = State::Done;
}

bool AttackUltimate::IsFinished() const {
    return m_state == State::Done;
}

bool AttackUltimate::IsInParryWindow() const {
    return fabsf(m_chargeTimer - m_params.laserDuration) <= m_params.parryWindow;
}

std::optional<XMFLOAT3> AttackUltimate::TryParry(const XMFLOAT3& parryPosition, Boss* boss) {
    if (m_state != State::Charging || !IsInParryWindow()) return std::nullopt;
    const Projectile* ball{ m_pool ? m_pool->Get(m_ball) : nullptr };
    if (!ball) return std::nullopt;

    // Read before shattering: ShatterBijuudama despawns the ball and may reuse its slot.
    const XMFLOAT3 ballPosition{ ball->position };
    ShatterBijuudama(parryPosition, boss);
    return ballPosition;
}

// ============================================================
// // ShatterBijuudama — Called by TryParry
// ============================================================

void AttackUltimate::ShatterBijuudama(XMFLOAT3 parryPos, Boss* boss) {
    if (!m_pool || !boss) return;

    m_pool->Despawn(m_ball);
    m_ball = ProjectileHandle{};

    CameraController::Instance().AddTrauma(0.8f);
    AudioManager::Instance().PlaySFX("Data/Sound/SE_Player_Parry_Boss.wav", 0.6f);

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<>   distCount(m_params.shatterMinFragments, m_params.shatterMaxFragments);
    std::uniform_real_distribution<float> distSize(m_params.shatterMinRadius, m_params.shatterMaxRadius);
    std::uniform_real_distribution<float> distDur(m_params.shatterMinDuration, m_params.shatterMaxDuration);
    std::uniform_real_distribution<float> distAngle(-XM_PIDIV2, XM_PIDIV2);

    int fragments = distCount(gen);

    XMFLOAT3 bossPos = boss->GetPosition();
    float baseAngle = atan2f(bossPos.x - parryPos.x, bossPos.z - parryPos.z);

    // Fragments curve back into the boss; CollisionManager damages it when they arrive.
    for (int spawned = 0; spawned < fragments; ++spawned) {
        Projectile* fragment{ m_pool->Get(m_pool->Spawn()) };
        if (!fragment) break; // pool is full

        fragment->position = parryPos;
        fragment->target = ProjectileTarget::boss;
        fragment->isParabolic = true;

        float r = distSize(gen);
        fragment->radius = r;
        fragment->visualScale = r * 3.0f;

        float spreadAngle = baseAngle + distAngle(gen);
        XMFLOAT3 ctrlPoint = {
            parryPos.x + sinf(spreadAngle) * m_params.shatterCurveOffset,
            parryPos.y,
            parryPos.z + cosf(spreadAngle) * m_params.shatterCurveOffset
        };
        fragment->parabolaStart = parryPos;
        fragment->parabolaControl = ctrlPoint;
        fragment->parabolaDuration = distDur(gen);
        AudioManager::Instance().PlaySFX(
            "Data/Sound/SE_Player_Parry_Boss.wav", 0.015f * m_params.sfxVolume);
    }

    CancelCharge();
    m_state = State::Recovering;
    m_recoveryTimer = 0.0f;
}

// ============================================================
// Internal
// ============================================================

void AttackUltimate::ClaimBall() {
    m_ball = m_pool->Spawn();
    Projectile* ball{ m_pool->Get(m_ball) };
    if (!ball) return; // pool is full: the attack runs without a ball

    // Parked far below the arena until the charge moves it to the boss.
    ball->position = { 0.0f, -1000.0f, 0.0f };
    ball->radius = m_params.baseHitbox;
    ball->visualScale = m_params.baseHitbox * m_params.visualMultiplier;
    ball->damage = m_params.ballDamage;
}

void AttackUltimate::LaunchBall(Boss* boss) {
    CancelCharge();

    m_state = State::Recovering;
    m_recoveryTimer = 0.0f;

    if (Projectile* ball{ m_pool->Get(m_ball) }) {
        XMFLOAT3 vel = ball->velocity;
        if ((vel.x * vel.x + vel.z * vel.z) < 0.01f) {
            XMFLOAT3 bPos = boss->GetPosition();
            XMFLOAT3 pPos = m_target->GetPosition();
            float dx = pPos.x - bPos.x;
            float dz = pPos.z - bPos.z;
            float dist = sqrtf(dx * dx + dz * dz);

            if (dist > 0.001f) {
                float spd = m_params.shootSpeed;
                ball->velocity = { (dx / dist) * spd, 0.0f, (dz / dist) * spd };
                AudioManager::Instance().PlaySFX(
                    "Data/Sound/SE_Boss_Bijuudama_Shoot.wav", 0.2f * m_params.sfxVolume);
            }
        }
    }
    m_ball = ProjectileHandle{};
}

void AttackUltimate::CancelCharge() {
    if (m_chargeEffectHandle != -1) {
        EffectManager::Instance().Stop(m_chargeEffectHandle);
        m_chargeEffectHandle = -1;
    }
    m_chargeTimer = 0.0f;
}