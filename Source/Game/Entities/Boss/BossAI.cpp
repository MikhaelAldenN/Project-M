#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "Boss.h"
#include "AttackParamManager.h"
#include <cmath>

#include "BossAI.h"
#include "BossPhase01.h"
#include "BossPhase02.h"
#include "Player.h"
#include "Boss.h"
#include "AttackParamManager.h"
#include <cmath>

// ========================================================
// BOSS AI - PHASE 01
// ========================================================
BossAI_Phase01::BossAI_Phase01(BossPhase01* phase, Player* target)
    : m_phase(phase), m_target(target) {}

// Tactician Mode: alternates main attacks with the filler.
BossAI_Phase01::AttackSequence BossAI_Phase01::GetNextTacticianAttack() {
    AttackSequence nextAttack;

    if (m_isNextMainAttack) {
        // Main attack rotation: Wave -> Phalanx -> Meteor -> Targeted Rain
        switch (m_mainAttackIndex) {
        case 0: nextAttack = AttackSequence::Wave; break;
        case 1: nextAttack = AttackSequence::Phalanx; break;
        case 2: nextAttack = AttackSequence::Meteor; break;
        case 3: nextAttack = AttackSequence::Rain; break;
        }
        m_mainAttackIndex = (m_mainAttackIndex + 1) % 4;
    }
    else {
        // The filler is always Radial; Direct and Fan are out of the filler rotation.
        nextAttack = AttackSequence::Radial;
    }

    m_isNextMainAttack = !m_isNextMainAttack;

    return nextAttack;
}

void BossAI_Phase01::Update(float dt, Boss* boss) {
    if (!m_enabled || !m_target || !m_phase) return;

    if (m_phase->HasActiveAttacks() || m_phase->HasRainActive()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        float hpPercent = static_cast<float>(m_phase->GetHP()) / static_cast<float>(m_phase->GetMaxHP());
        bool isEnraged = (hpPercent <= 0.5f);

        // One-time transition when the boss enters Chaos Mode (HP <= 50%)
        if (isEnraged && !m_isEnraged) {
            m_isEnraged = true;
            m_currentAttack = AttackSequence::Phalanx; // the Chaos cycle starts with Phalanx
        }

        DirectX::XMFLOAT3 pPos = m_target->GetPosition();
        DirectX::XMFLOAT3 bPos = boss->GetPosition();
        float lockedAngle = std::atan2f(pPos.x - bPos.x, pPos.z - bPos.z);

        switch (m_currentAttack) {

            // ========================================================
            // FILLER ATTACK (Tactician Mode)
            // Direct and Fan are out of the active filler rotation and are not
            // reachable from GetNextTacticianAttack() or the default state.
            // The cases are kept as possible layers for Phase 2 combinations.
            // ========================================================
        case AttackSequence::Direct:
            m_phase->AddAttack(std::make_unique<AttackDirect>(AttackParamManager::Instance().GetDirectParams(), m_target));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Fan:
            m_phase->AddAttack(std::make_unique<AttackFan>(AttackParamManager::Instance().GetFanNormalParams(), lockedAngle, m_target));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::Radial:
            m_phase->AddAttack(std::make_unique<AttackRadial>(AttackParamManager::Instance().GetRadialNormalParams()));
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

            // ========================================================
            // SHARED MAIN ATTACKS (Tactician = Single, Enraged = Combo + Rain)
            // ========================================================
        case AttackSequence::Phalanx:
            m_phase->AddPhalanx(std::make_unique<AttackPhalanx>(AttackParamManager::Instance().GetPhalanxParams(), m_target));
            if (isEnraged) {
                // Chaos Mode: a Rain runs alongside every main attack.
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep, m_target->GetPosition().x > 0);
                m_currentAttack = AttackSequence::Wave; // next attack in the Chaos cycle
            }
            else {
                m_currentAttack = GetNextTacticianAttack();
            }
            m_cooldownTimer = 1.0f;
            break;

        case AttackSequence::Wave:
            m_phase->AddAttack(std::make_unique<AttackWave>(AttackParamManager::Instance().GetWaveParams()));
            if (isEnraged) {
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep, m_target->GetPosition().x > 0);
                m_currentAttack = AttackSequence::Meteor;
            }
            else {
                m_currentAttack = GetNextTacticianAttack();
            }
            m_cooldownTimer = 1.0f;
            break;

        case AttackSequence::Meteor:
            m_phase->AddAttack(std::make_unique<AttackMeteor>(AttackParamManager::Instance().GetMeteorParams()));
            if (isEnraged) {
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep, m_target->GetPosition().x > 0);
                m_currentAttack = AttackSequence::Ultimate;
            }
            else {
                m_currentAttack = GetNextTacticianAttack();
            }
            m_cooldownTimer = 1.0f;
            break;

        case AttackSequence::Ultimate:
            m_phase->AddUltimate(std::make_unique<AttackUltimate>(AttackParamManager::Instance().GetUltimateParams(), m_target));
            if (isEnraged) {
                m_phase->TriggerRain(AttackParamManager::Instance().GetRainParams(), RainMode::VerticalSweep, m_target->GetPosition().x > 0);
                m_currentAttack = AttackSequence::Phalanx; // back to the start of the Chaos cycle
            }
            else {
                m_currentAttack = GetNextTacticianAttack();
            }
            m_cooldownTimer = 2.5f;
            break;

            // ========================================================
            // THE RAIN MANAGER (Tactician = Targeted)
            // ========================================================
        case AttackSequence::Rain:
            // Only Tactician Mode reaches this state; in Chaos Mode the Rain comes with each main attack.
            m_phase->TriggerRain(AttackParamManager::Instance().GetRainTargetedParams(), RainMode::Targeted, true);
            m_cooldownTimer = 1.0f;
            m_currentAttack = GetNextTacticianAttack();
            break;

        case AttackSequence::RadialContinuos:
        case AttackSequence::FanContinuos:
            // Not scheduled by either mode.
            break;
        }
    }
}

// ========================================================
// BOSS AI - PHASE 02 (WINDOWKILL)
// ========================================================
BossAI_Phase02::BossAI_Phase02(BossPhase02* phase, Player* target)
    : m_phase(phase), m_target(target) {}

void BossAI_Phase02::Update(float dt, Boss* boss) {
    // No attacks while the player is still caged.
    if (!m_enabled || !m_target || !m_phase || m_phase->IsPlayerCaged()) return;

    if (m_phase->HasActiveAttacks()) return;

    m_cooldownTimer -= dt;

    if (m_cooldownTimer <= 0.0f) {
        // Rotation: Bouncing -> Boomerang -> Blaster -> Spear
        switch (m_currentAttack) {
        case AttackSequence::Bouncing:
            m_phase->AddAttack(std::make_unique<AttackBouncing>(AttackParamManager::Instance().GetBouncingParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Boomerang;
            break;

        case AttackSequence::Boomerang:
            m_phase->AddAttack(std::make_unique<AttackBoomerangs>(AttackParamManager::Instance().GetBoomerangParams()));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Blaster;
            break;

        case AttackSequence::Blaster:
            // Blasters aimed at the player's current X
            m_phase->AddAttack(std::make_unique<AttackBlasters>(AttackParamManager::Instance().GetBlasterParams(), true, m_target->GetPosition().x));
            m_cooldownTimer = 2.0f;
            m_currentAttack = AttackSequence::Spear;
            break;

        case AttackSequence::Spear:
            m_phase->AddAttack(std::make_unique<AttackSpears>(AttackParamManager::Instance().GetUndyneParams(), m_target));
            m_cooldownTimer = 2.5f;
            m_currentAttack = AttackSequence::Bouncing;
            break;
        }
    }
}