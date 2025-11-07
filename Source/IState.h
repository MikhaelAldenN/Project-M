#pragma once

// Ths is the interface (abstract base class) for all states in the State Machine
class IState
{
public:
    virtual ~IState() {}

    virtual void OnEnter() = 0;
    virtual void Update(float delta_time) = 0;
    virtual void OnExit() = 0;

	// Returns the name of the state for debugging purposes
    virtual const char* GetName() const = 0;
};