#include "PlayerState_Jump.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"

void PlayerState_Jump::OnEnter()
{
    // 1. Inisialisasi state internal
    isFalling = false; // Setiap kali masuk state, kita belum jatuh
    isSprintJump = false; // Asumsi awal bukan sprint jump

    // 2. Terapkan gaya lompat DAN mainkan animasi yang sesuai
    if (player->IsSprinting())
    {
        // --- INI ADALAH SPRINT JUMP ---
        isSprintJump = true; // Tandai ini sebagai sprint jump

        // Mainkan animasi jump_start (non-looping)
        player->GetAnimation()->PlayAnimation(player->GetModel(), "jump_start", false); // <-- MODIFIKASI

        // Terapkan gaya lompat sprint
        player->Jump(10.0f); // Gunakan nilai jumpSpeed dari Player sebaiknya
        player->SetMoveSpeed(25.0f); // Atur kecepatan gerak di udara jika perlu
    }
    else
    {
        // --- INI ADALAH JUMP BIASA ---
        isSprintJump = false;

        // Mainkan animasi lompat biasa (non-looping)
        player->GetAnimation()->PlayAnimation(player->GetModel(), "jump", false); // <-- Ini dari kode asli Anda

        // Terapkan gaya lompat biasa
        player->Jump(12.0f); // Gunakan nilai jumpSpeed dari Player sebaiknya
    }
}

void PlayerState_Jump::Update(float delta_time)
{
    // OPSI: Air Control
    player->InputMove(delta_time);

    // --- LOGIKA BARU UNTUK TRANSISI JUMP_FALL ---
    // Cek apakah ini sprint jump, belum ditandai jatuh, DAN velocity Y sudah negatif (mulai jatuh)
    if (isSprintJump && !isFalling && player->GetVelocity().y <= 0.0f)
    {
        isFalling = true; // Tandai sudah jatuh agar tidak memanggil ini lagi
        player->GetAnimation()->PlayAnimation(player->GetModel(), "jump_fall", true); // Mainkan animasi jatuh (looping)
    }
    // --- AKHIR LOGIKA BARU ---

    // Cek apakah sudah mendarat
    if (player->GetVelocity().y <= 0.0f && player->IsOnGround())
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
    // Kembalikan kecepatan gerak normal di darat
    player->SetMoveSpeed(7.0f);
}