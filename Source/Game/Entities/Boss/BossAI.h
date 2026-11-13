#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>

// --- Phase 1 Attacks ---
#include "AttackRadial.h"
#include "AttackFan.h"
#include "AttackPhalanx.h"
#include "AttackRain.h"
#include "AttackUltimate.h"
#include "AttackWave.h"
#include "AttackMeteor.h"
#include "AttackDirect.h"

// --- Phase 2 Attacks ---
#include "AttackBouncing.h"
#include "AttackBoomerangs.h"
#include "AttackBlasters.h"
#include "AttackSpears.h"
#include "AttackWall.h"

class BossPhase01;
class BossPhase02;
class CueBoss;
class Player;

// ========================================================
// BOSS AI - PHASE 01 (Act 2)
// ========================================================
// Runs a fixed rotation per fight phase: phase A above 50% HP, phase B at or below.
class BossAI_Phase01 {
public:
    // Attacks the Act 2 rotation can launch.
    enum class AttackSequence {
        Radial,       // also stands in for Mekar until that attack exists
        Fan,          // filler (FanContinuous set), re-aimed at the player on every wave
        Phalanx,      // starts its own HorizontalSweep Rain (BossPhase01::AddPhalanx)
        RainTargeted,
        Meteor,
        Ultimate      // old ultimate, stands in for the new one; starts its own DualPillar Rain
    };

    // One entry of a rotation. withRain adds a VerticalSweep Rain after the attack;
    // the phase ignores it while another Rain is running.
    struct RotationStep {
        AttackSequence attack{ AttackSequence::Radial };
        bool withRain{ false };
        RainMode comboRain{ RainMode::VerticalSweep };
    };

    BossAI_Phase01(BossPhase01* phase, Player* target);
    void Update(float dt, CueBoss* boss);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void SetTarget(Player* target) { m_target = target; }

private:
    // Starts `attack` on the phase and returns the cooldown before the next step.
    // `boss` is non-null (checked by the phase before Update).
    float LaunchAttack(AttackSequence attack, CueBoss* boss);

    BossPhase01* m_phase = nullptr;
    Player* m_target = nullptr;
    bool         m_enabled = false;

    bool  m_isPhaseB = false;   // set once HP is at or below 50%
    int   m_stepIndex = 0;      // next step in the active rotation
    float m_cooldownTimer = 2.0f;
};

// ========================================================
// BOSS AI - PHASE 02 (WINDOWKILL)
// ========================================================
class BossAI_Phase02 {
public:

    enum class AttackSequence {
        Wall,
        Bouncing,
        Spear,
        Blaster
    };

    BossAI_Phase02(BossPhase02* phase, Player* target);
    void Update(float dt, CueBoss* boss);

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }
    void SetTarget(Player* target) { m_target = target; }

private:
    BossPhase02* m_phase = nullptr;
    Player* m_target = nullptr;
    bool          m_enabled = false;

    // Next attack in the fixed rotation; the phase opens with the wall.
    AttackSequence m_currentAttack = AttackSequence::Wall;
    float m_cooldownTimer = 0.5f; // USULAN DESAIN: delay after the opening dialogue before the first wall
};