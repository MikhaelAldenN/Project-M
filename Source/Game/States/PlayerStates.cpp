#include "PlayerStates.h"
#include "Player.h"
#include "PlayerConstants.h"
#include "StateMachine.h"
#include "AnimationController.h"
#include "System/Input.h"
#include <memory>
#include <cmath>

#include "System/AudioManager.h"

#include "EffectManager.h"

namespace {
    // Player bindings. Keyboard and mouse buttons are virtual-key codes read through Keyboard.
    // Charged shot and ult are reserved here so no other action takes them; nothing reads them yet.
    namespace Binding
    {
        constexpr int keyShoot{ VK_LBUTTON };
        constexpr int keyDash{ VK_SPACE };
        constexpr int keyChargedShot{ VK_SHIFT };
        constexpr int keyUlt{ 'E' };

        // Gamepad: shoot is the right stick and charged shot is the right trigger (both analog).
        constexpr GamePadButton padDash{ GamePad::BTN_A };
        constexpr GamePadButton padUlt{ GamePad::BTN_Y };
    }

    [[nodiscard]] bool IsDashInputTriggered() noexcept
    {
        auto& input{ Input::Instance() };

        const bool isKeyboardDash{ input.GetKeyboard().IsTriggered(Binding::keyDash) };
        const bool isGamepadDash{ (input.GetGamePad().GetButtonDown() & Binding::padDash) != 0 };

        return isKeyboardDash || isGamepadDash;
    }

    [[nodiscard]] bool IsShootInputPressed(const Player& player) noexcept
    {
        auto& input{ Input::Instance() };

        const bool isMouseShoot{ input.GetKeyboard().IsPress(Binding::keyShoot) };

        // Why a threshold above the aim deadzone: a light tilt aims without firing.
        const GamePad& gamePad{ input.GetGamePad() };
        const float rx{ gamePad.GetAxisRX() };
        const float ry{ gamePad.GetAxisRY() };
        const float threshold{ player.GetStickShootThreshold() };
        const bool isStickShoot{ ((rx * rx) + (ry * ry)) > (threshold * threshold) };

        return isMouseShoot || isStickShoot;
    }

    // Enters PlayerShoot while the shoot input is held. The first shot fires on the
    // press frame when the fire interval allows it.
    [[nodiscard]] bool TryStartShoot(Player* player, float dt)
    {
        if (!player || !player->IsInputEnabled()) return false;
        if (!IsShootInputPressed(*player)) return false;

        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerShoot>());
        player->TryFire(dt);
        return true;
    }
}

using namespace DirectX;

// ============================================================
// IDLE
// ============================================================
void PlayerIdle::Enter(Player* player)
{
    player->GetAnimator()->SetPlaybackSpeed(1.0f);
    player->GetAnimator()->Play("Idle", true, PlayerConst::AnimBlendDefault);
}

void PlayerIdle::Update(Player* player, float dt)
{
    if (!player->IsInputEnabled()) return;

    // Dash Priority (Seamlessly checks Keyboard and Gamepad LB)
    if (IsDashInputTriggered()) 
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // Shoot
    if (TryStartShoot(player, dt)) return;

    // Movement Fallback
    if (player->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
    }
}

// ============================================================
// MOVING
// ============================================================
void PlayerMoving::Enter(Player* player)
{
    player->GetAnimator()->Play("RunPistol", true, PlayerConst::AnimBlendDefault);
}

void PlayerMoving::Update(Player* player, float dt)
{
    // Handle Animation direction
    player->GetAnimator()->SetPlaybackSpeed(player->IsBackpedaling() ? -1.0f : 1.0f);

    if (player->IsInputEnabled())
    {
        // Dash Priority
        if (IsDashInputTriggered()) 
        {
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
            return;
        }

        // Shoot
        if (TryStartShoot(player, dt)) return;
    }

    // Idle Fallback
    if (!player->IsMoving())
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}
// ============================================================
// DASH
// ============================================================

void PlayerDash::Enter(Player* player)
{
    m_timer = player->GetDashDuration();
    m_dashDir = player->GetLastValidInput();

    // Why normalized: gamepad input keeps the stick tilt, which would shorten the dash.
    const float dirLength{ std::sqrt((m_dashDir.x * m_dashDir.x) + (m_dashDir.y * m_dashDir.y)) };
    m_dashDir = (dirLength > 0.0001f)
        ? DirectX::XMFLOAT2{ m_dashDir.x / dirLength, m_dashDir.y / dirLength }
    : DirectX::XMFLOAT2{ 0.0f, 1.0f };

    // Player owns the charge rule: it grants invincibility on a full dash and returns
    // a reduced multiplier on a penalized one.
    m_speedScale = player->BeginDash();

    // Why here and not in Update: movement runs before the state machine in
    // Player::Update, so a velocity set in Update would only move the player two frames
    // after the press.
    const float speed{ player->GetDashSpeed() * m_speedScale };
    player->GetMovement()->SetVelocity({ m_dashDir.x * speed, 0.0f, m_dashDir.y * speed });
    player->SetStateMoveTimeLimit(m_timer);

    // =========================================================
    // Dash Go VFX, turned to the dash direction
    // =========================================================
    DirectX::XMFLOAT3 pos = player->GetMovement()->GetPosition();
    pos.y += 1.0f; // raised to the middle of the body

    // Keep the handle so the VFX can follow the player during the dash.
    m_dashGoVfxHandle = EffectManager::Instance().Play("Data/Effect/VFX_Player_Dash_Go.efk", pos, 0.2f);

    if (m_dashGoVfxHandle != -1) {
        // Add XM_PI (180 degrees) to reverse the effect's direction.
        float yaw = atan2f(m_dashDir.x, m_dashDir.y) + DirectX::XM_PI;

        EffectManager::Instance().SetRotation(m_dashGoVfxHandle, { 0.0f, yaw, 0.0f });
    }

    std::string dashSounds[] = {
        "Data/Sound/SE_Dash_01.wav",
        "Data/Sound/SE_Dash_02.wav",
        "Data/Sound/SE_Dash_03.wav"
    };

    // One of three dash sounds at random
    int randomIndex = rand() % 3;

    // kept quiet (0.1) on purpose
    AudioManager::Instance().PlaySFX(dashSounds[randomIndex], 0.1f);
}

void PlayerDash::Update(Player* player, float dt)
{
    m_timer -= dt;

    // Why buffered: the dash input is an edge trigger, so a press during the dash would
    // otherwise be lost and two back-to-back dashes would need frame-perfect timing.
    if (player->IsInputEnabled() && IsDashInputTriggered()) m_isDashBuffered = true;

    // =========================================================
    // Keep the VFX on the player for the whole dash
    // =========================================================
    if (m_dashGoVfxHandle != -1 && EffectManager::Instance().IsPlaying(m_dashGoVfxHandle))
    {
        DirectX::XMFLOAT3 trackPos = player->GetMovement()->GetPosition();
        trackPos.y += 1.0f; // same Y offset as in Enter
        EffectManager::Instance().SetPosition(m_dashGoVfxHandle, trackPos);
    }

    if (m_timer <= 0.0f)
    {
        // A buffered press chains into the next dash; its Enter asks Player::BeginDash
        // whether that dash is full or penalized.
        if (m_isDashBuffered)
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        else if (player->IsMoving())
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
        else
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
    }
}

void PlayerDash::Exit(Player* player)
{
    player->GetMovement()->SetVelocity({ 0.0f, 0.0f, 0.0f });
    player->ClearStateMoveTimeLimit();
}

// ============================================================
// SHOOT
// ============================================================

void PlayerShoot::Update(Player* player, float dt)
{
    // Dash Lockout Prevention
    if (IsDashInputTriggered()) // always allowed; Player::BeginDash decides full or penalized
    {
        player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerDash>());
        return;
    }

    // Why no minimum time in this state: the fire interval lives in Player, so leaving
    // and re-entering cannot fire faster than holding.
    if (!IsShootInputPressed(*player))
    {
        if (player->IsMoving())
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerMoving>());
        else
            player->GetStateMachine()->ChangeState(player, std::make_unique<PlayerIdle>());
        return;
    }

    // Lower-Body Animation Sync
    if (player->IsMoving()) {
        player->GetAnimator()->SetPlaybackSpeed(player->IsBackpedaling() ? -1.0f : 1.0f);
        if (!player->GetAnimator()->IsPlaying("RunPistol")) {
            player->GetAnimator()->Play("RunPistol", true, PlayerConst::AnimBlendDefault);
        }
    }
    else {
        player->GetAnimator()->SetPlaybackSpeed(1.0f);
        if (!player->GetAnimator()->IsPlaying("Idle")) {
            player->GetAnimator()->Play("Idle", true, PlayerConst::AnimBlendDefault);
        }
    }

    player->TryFire(dt);
}