#include "PlayerState_Run.h"
#include "PlayerState_Idle.h"
#include "PlayerState_Sprint.h"
#include "PlayerState_Jump.h"

void PlayerState_Run::OnEnter() 
{
	// Set animation, functions, etc. for run state here
	player->GetAnimation()->PlayAnimation(player->GetModel(), "run", true);

}

void PlayerState_Run::Update(float delta_time) 
{
    // PANGGIL GERAKAN DI SINI!
    player->InputMove(delta_time);

    if (Input::Instance().GetGamePad().GetButtonDown() & GamePad::BTN_A) {
        player->GetStateMachine()->ChangeState(new PlayerState_Jump(player));
        return;
    }

    if (!player->IsRunning()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Idle(player));
		return;
    }
    // Tambahkan cek transisi ke Sprint
    if (player->IsSprinting()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Sprint(player));
        return;
    }
}

void PlayerState_Run::OnExit() 
{
	// cleanup if needed
}