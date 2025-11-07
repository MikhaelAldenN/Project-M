#pragma once

#include "IState.h"
#include "Player.h"

class PlayerState_Run : public IState 
{
public:
    PlayerState_Run(Player* player) : player(player) {}
    void OnEnter() override;
    void Update(float delta_time) override;
    void OnExit() override;
    const char* GetName() const { return "Run"; }

private:
    Player* player;
};