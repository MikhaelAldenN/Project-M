#include "PlayerState_Idle.h"
#include "PlayerState_Run.h"

void PlayerState_Idle::OnEnter() 
{ 
    // Put animation, function, etc. for idle state here
}
void PlayerState_Idle::Update(float delta_time) 
{
    if (player->IsMoving()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Run(player));
		return;
    }
	// Other logic for idle state
}

void PlayerState_Idle::OnExit() { /* cleanup if needed */ }