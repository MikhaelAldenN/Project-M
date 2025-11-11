#include "PlayerState_Jump.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"

void PlayerState_Jump::OnEnter()
{
    // 1. Mainkan animasi lompat (looping false karena kita mungkin punya animasi landing terpisah nanti)
    player->GetAnimation()->PlayAnimation(player->GetModel(), "jump", false);

    // 2. Terapkan gaya lompat SEKALI SAJA saat masuk state
    // Asumsi fungsi Jump(speed) ada di Player public
    player->Jump(12.0f); // Gunakan nilai jumpSpeed dari Player sebaiknya
}

void PlayerState_Jump::Update(float delta_time)
{
    // OPSI: Apakah pemain boleh mengontrol arah saat di udara? (Air Control)
    // Jika YA, panggil player->InputMove(delta_time) di sini juga.
    // Biasanya Hack n Slash kasih sedikit air control.
    player->InputMove(delta_time); // Uncomment jika mau air control

    // Cek apakah sudah mendarat
    // Asumsi: Velocity Y negatif artinya sedang jatuh, DAN posisi Y sudah di tanah
    if (player->GetVelocity().y <= 0.0f && player->IsOnGround()) // Butuh implementasi IsOnGround() yang presisi
    {
        // Jika mendarat sambil bergerak -> Run, jika diam -> Idle
        if (player->IsRunning()) {
            player->GetStateMachine()->ChangeState(new PlayerState_Run(player));
        }
        else {
            player->GetStateMachine()->ChangeState(new PlayerState_Idle(player));
        }
        return;
    }
}

void PlayerState_Jump::OnExit()
{
    // Mungkin mainkan sfx mendarat?
}