#pragma once

// Forward declaration untuk menghindari include sirkular jika IState butuh StateMachine
class IState;

class StateMachine
{
public:
    StateMachine();
    ~StateMachine();

    void ChangeState(IState* newState);
    void Update(float delta_time);

    IState* GetCurrentState() const;

private:

    IState* currentState;

    // Mencegah copy constructor dan assignment operator
    // FSM tidak boleh di-copy secara sembarangan
    StateMachine(const StateMachine&) = delete;
    StateMachine& operator=(const StateMachine&) = delete;
};
