#pragma once
#include "IState.h"
#include "Player.h"

class PlayerState_Jump : public IState
{
public:
    PlayerState_Jump(Player* player) : player(player) {}
    void OnEnter() override;
    void Update(float delta_time) override;
    void OnExit() override;
    const char* GetName() const override { return "Jump"; }

private:
    Player* player;
};