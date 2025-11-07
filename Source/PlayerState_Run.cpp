#include "PlayerState_Run.h"
#include "PlayerState_Idle.h"

void PlayerState_Run::OnEnter() 
{
	// Set animation, functions, etc. for run state here
}

void PlayerState_Run::Update(float delta_time) 
{
    if (!player->IsMoving()) {
        player->GetStateMachine()->ChangeState(new PlayerState_Idle(player));
		return;
    }
	// Other logic for run state
}

void PlayerState_Run::OnExit() 
{
	// cleanup if needed
}