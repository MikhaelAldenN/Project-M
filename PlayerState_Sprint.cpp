#include "PlayerState_Sprint.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"

void PlayerState_Sprint::OnEnter()
{
    // Ganti animasi ke sprint
    player->GetAnimation()->PlayAnimation(player->GetModel(), "sprint", true);

    // Naikkan kecepatan gerak
    // Idealnya angka ini jangan hardcoded, tapi ambil dari konstanta di Player.
    player->SetMoveSpeed(20.0f);
}

void PlayerState_Sprint::Update(float delta_time)
{
    // Jika berhenti bergerak sama sekali -> Idle
    if (!player->IsRunning())
    {
        player->GetStateMachine()->ChangeState(new PlayerState_Idle(player));
        return;
    }

    // Jika masih bergerak TAPI tombol sprint dilepas -> Run
    if (!player->IsSprinting())
    {
        player->GetStateMachine()->ChangeState(new PlayerState_Run(player));
        return;
    }
}

void PlayerState_Sprint::OnExit()
{
    // Kembalikan kecepatan ke normal saat keluar dari state Sprint
    player->SetMoveSpeed(7.0f);
}