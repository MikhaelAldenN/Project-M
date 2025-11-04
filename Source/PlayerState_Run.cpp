#include "PlayerState_Run.h"
#include "PlayerState_Idle.h"

void PlayerState_Run::OnEnter() 
{
    /* atur animasi lari, dsb */ 
}

void PlayerState_Run::Update(float delta_time) 
{
    if (!player->IsMoving()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Idle(player));
		return;
    }
    // logika run lain
}

void PlayerState_Run::OnExit() 
{
    /* cleanup jika perlu */ 
}