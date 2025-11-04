#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"

void PlayerState_Idle::OnEnter() 
{ 
    /* bisa atur animasi idle, dsb */ 
}
void PlayerState_Idle::Update(float delta_time) 
{
    if (player->IsMoving()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Run(player));
		return;
    }
    // logika idle lain
}

void PlayerState_Idle::OnExit() { /* cleanup jika perlu */ }