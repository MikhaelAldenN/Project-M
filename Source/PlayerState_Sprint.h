#pragma once

#include "IState.h"
#include "Player.h"

class PlayerState_Sprint : public IState
{
public:
    PlayerState_Sprint(Player* player) : player(player) {}
    void OnEnter() override;
    void Update(float delta_time) override;
    void OnExit() override;
    const char* GetName() const override { return "Sprint"; }

private:
    Player* player;
};