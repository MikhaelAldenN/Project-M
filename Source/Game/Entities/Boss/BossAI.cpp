#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "CueBoss.h"
#include "AttackParamManager.h"
#include <array>
#include <cmath>

// ========================================================
// BOSS AI - PHASE 01 (Act 2)
// ========================================================
namespace
{
    using Attack = BossAI_Phase01::AttackSequence;
    using Step = BossAI_Phase01::RotationStep;

    // USULAN DESAIN: Act 2 rotation. The first slot belongs to Mekar; Radial stands in.
    constexpr std::array<Step, 8> k_phaseARotation{ {
        { Attack::Radial, false },       { Attack::Fan, false },
        { Attack::RainTargeted, false }, { Attack::Fan, false },
        { Attack::Meteor, false },       { Attack::Fan, false },
        { Attack::Phalanx, false },      { Attack::Fan, false },
    } };

    // Phase B: the same order with an area-denial Rain on every attack, fillers included,
    // then the ultimate. RainTargeted, Phalanx and Ultimate already run a Rain, so they
    // get no second one.
    constexpr std::array<Step, 10> k_phaseBRotation{ {
        { Attack::Radial, true },        { Attack::Fan, true },
        { Attack::RainTargeted, false }, { Attack::Fan, true },
        { Attack::Meteor, true },        { Attack::Fan, true },
        { Attack::Phalanx, false },      { Attack::Fan, true },
        { Attack::Ultimate, false },     { Attack::Fan, true },
    } };
}

BossAI_Phase01::BossAI_Phase01(BossPhase01* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase01::Update(float dt, CueBoss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    if (m_phase->HasActiveAttacks() || m_phase->HasRainActive()) return;

    m_cooldownTimer -= dt;
    if (m_cooldownTimer > 0.0f) return;

    // One-time switch to phase B; its rotation starts from the top.
    const float hpFraction{ static_cast<float>(m_phase->GetHP()) / static_cast<float>(m_phase->GetMaxHP()) };
    if (!m_isPhaseB && hpFraction <= 0.5f) {
        m_isPhaseB = true;
        m_stepIndex = 0;
    }

    const Step* rotation{ m_isPhaseB ? k_phaseBRotation.data() : k_phaseARotation.data() };
    const int rotationSize{ static_cast<int>(m_isPhaseB ? k_phaseBRotation.size() : k_phaseARotation.size()) };
    const Step step{ rotation[m_stepIndex] };
    m_stepIndex = (m_stepIndex + 1) % rotationSize;

    m_cooldownTimer = LaunchAttack(step.attack, boss);
    if (step.withRain) {
        // The zone covers the half of the arena the player stands in.
        m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep,
            m_target->GetPosition().x > 0.0f);
    }
}

float BossAI_Phase01::LaunchAttack(AttackSequence attack, CueBoss* boss) {
    AttackParamManager& params{ AttackParamManager::Instance() };

    switch (attack) {
    case AttackSequence::Radial:
        m_phase->AddAttack(std::make_unique<AttackRadial>(params.GetRadialNormalParams()));
        return 1.0f;

    case AttackSequence::Fan:
    {
        // First aim; with a target the fan re-aims at the player on every wave.
        const DirectX::XMFLOAT3 playerPos{ m_target->GetPosition() };
        const DirectX::XMFLOAT3 bossPos{ boss->GetPosition() };
        const float lockedAngle{ std::atan2f(playerPos.x - bossPos.x, playerPos.z - bossPos.z) };
        m_phase->AddAttack(std::make_unique<AttackFan>(params.GetFanContinuousParams(), lockedAngle, m_target));
        return 1.0f;
    }

    case AttackSequence::Phalanx:
        m_phase->AddPhalanx(std::make_unique<AttackPhalanx>(params.GetPhalanxParams(), m_target));
        return 1.0f;

    case AttackSequence::RainTargeted:
        m_phase->TriggerRain(params.GetRainTargetedParams(), RainMode::Targeted, true);
        return 1.0f;

    case AttackSequence::Meteor:
        m_phase->AddAttack(std::make_unique<AttackMeteor>(params.GetMeteorParams(), m_target->GetPosition()));
        return 1.0f;

    case AttackSequence::Ultimate:
        m_phase->AddUltimate(std::make_unique<AttackUltimate>(params.GetUltimateParams(), m_target));
        return 2.5f;
    }
    return 1.0f;
}

// ========================================================
// BOSS AI - PHASE 02 (WINDOWKILL)
// ========================================================
BossAI_Phase02::BossAI_Phase02(BossPhase02* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase02::Update(float dt, CueBoss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    if (m_phase->HasActiveAttacks()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // Rotation: Wall -> Bouncing -> Spear -> Blaster, then again from Wall.
        switch (m_currentAttack) {
        case AttackSequence::Wall:
            m_phase->AddAttack(std::make_unique<AttackWall>(AttackParamManager::Instance().GetWallParams(), m_target));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Bouncing;
            break;

        case AttackSequence::Bouncing:
            m_phase->AddAttack(std::make_unique<AttackBouncing>(AttackParamManager::Instance().GetBouncingParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Spear;
            break;

        case AttackSequence::Spear:
            m_phase->AddAttack(std::make_unique<AttackSpears>(AttackParamManager::Instance().GetUndyneParams(), m_target));
            m_cooldownTimer = 2.5f;
            m_currentAttack = AttackSequence::Blaster;
            break;

        case AttackSequence::Blaster:
            m_phase->AddAttack(std::make_unique<AttackBlasters>(AttackParamManager::Instance().GetBlasterParams(), m_target));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Wall;
            break;
        }
    }
}