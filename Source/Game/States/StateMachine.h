#pragma once
#include <memory>
#include "PlayerState.h"

class StateMachine
{
public:
    // currentState cleaned up automatically by unique_ptr
    ~StateMachine() = default;

    // Takes ownership of startState and immediately enters it
    void Initialize(std::unique_ptr<PlayerState> startState, Player* player)
    {
        m_currentState = std::move(startState);
        m_currentState->Enter(player);
    }

    // Exits current state, takes ownership of newState, enters it
    void ChangeState(Player* player, std::unique_ptr<PlayerState> newState)
    {
        if (m_currentState)
            m_currentState->Exit(player);

        m_currentState = std::move(newState);
        m_currentState->Enter(player);
    }

    void Update(Player* player, float elapsedTime)
    {
        if (m_currentState)
            m_currentState->Update(player, elapsedTime);
    }

private:
    std::unique_ptr<PlayerState> m_currentState;
};