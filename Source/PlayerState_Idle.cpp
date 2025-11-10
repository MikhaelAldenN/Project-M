#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"
#include "PlayerState_Sprint.h"

void PlayerState_Idle::OnEnter() 
{ 
    // Put animation, function, etc. for idle state here
    player->GetAnimation()->PlayAnimation(player->GetModel(), "idle", true);
}
void PlayerState_Idle::Update(float delta_time) 
{
    // Cek Sprint DULUAN sebelum Run.
    // Jika IsRunning true DAN IsSprinting true, dia akan masuk sini.
    if (player->IsSprinting()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Sprint(player));
        return;
    }

    if (player->IsRunning()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Run(player));
		return;
    }
}

void PlayerState_Idle::OnExit() { /* cleanup if needed */ }