#pragma once
#include <string>

class Player;

// --- 1. Abstract Base State ---
class PlayerState
{
public:
    virtual ~PlayerState() = default;

    // Called once when the state starts (plays its animation).
    virtual void Enter(Player* player) = 0;

    // Logic per frame (input handling, check transition)
    virtual void Update(Player* player, float elapsedTime) = 0;

    // Called once when the state ends (cleanup, if any).
    virtual void Exit(Player* player) = 0;
};