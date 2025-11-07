#include "StateMachine.h"
#include "IState.h"
#include <cstddef>

StateMachine::StateMachine() : currentState(nullptr)
{

}

StateMachine::~StateMachine()
{
	// Call ChangeState with nullptr to clean up the current state
    ChangeState(nullptr);
}

void StateMachine::ChangeState(IState* newState)
{
	// 1. Cleanup current state
    if (currentState != nullptr)
    {
        currentState->OnExit();
        delete currentState;
        currentState = nullptr;
    }

	// 2. Set the new state
    currentState = newState;

	// 3. Initialize the new state
    if (currentState != nullptr)
    {
        currentState->OnEnter();
    }
}

void StateMachine::Update(float delta_time)
{
    if (currentState != nullptr)
    {
        currentState->Update(delta_time);
    }
}

IState* StateMachine::GetCurrentState() const
{
    return currentState;
}
