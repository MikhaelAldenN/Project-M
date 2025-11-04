#pragma once
#pragma once
#include "IState.h"
#include "Player.h"

class PlayerState_Idle : public IState 
{
public:
    PlayerState_Idle(Player* player) : player(player) {}
    void OnEnter() override;
    void Update(float delta_time) override;
    void OnExit() override;
    const char* GetName() const { return "Idle"; }

private:
    Player* player;
};