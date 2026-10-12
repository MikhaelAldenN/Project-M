#pragma once

#include <DirectXMath.h>
#include <functional>
#include <utility>

class Player;
class Stage;

// Fires level events when the player stands on one of the stage's trigger lines.
// The checkpoint callback receives the line's center; the enable-line callback receives
// the line's index in Stage::m_linesEnable. Both fire every frame while the player stays on the line.
class LevelTriggerSystem
{
public:
    // Both pointers are borrowed. The caller keeps them alive for as long as Update() is called.
    void Initialize(const Player* player, const Stage* stage);

    void SetOnCheckpointReached(std::function<void(DirectX::XMFLOAT3)> callback) { m_onCheckpointReached = std::move(callback); }
    void SetOnEnableLineReached(std::function<void(int)> callback) { m_onEnableLineReached = std::move(callback); }

    // Does nothing while the player is dead or missing.
    void Update() const;

private:
    const Player* m_player{ nullptr };
    const Stage* m_stage{ nullptr };

    std::function<void(DirectX::XMFLOAT3)> m_onCheckpointReached{};
    std::function<void(int)> m_onEnableLineReached{};
};