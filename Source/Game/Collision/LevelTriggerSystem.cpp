#include "Game/Collision/LevelTriggerSystem.h"

#include "Player.h"
#include "Stage.h"

using namespace DirectX;

namespace
{
    // Half-depth of a trigger line along its local Z axis.
    constexpr float kLineTriggerRangeZ{ 2.0f };

    // True when the point lies within the line's length (local X) and trigger depth (local Z).
    bool IsOnLine(const XMFLOAT3& worldPos, const DebugLineData& line)
    {
        // Rotation matrices are orthonormal, so the transpose is the inverse.
        const XMVECTOR relative = XMVectorSubtract(XMLoadFloat3(&worldPos), XMLoadFloat3(&line.Position));
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(line.Rotation.x),
            XMConvertToRadians(line.Rotation.y),
            XMConvertToRadians(line.Rotation.z));

        XMFLOAT3 local{};
        XMStoreFloat3(&local, XMVector3TransformNormal(relative, XMMatrixTranspose(rotation)));

        const float halfLength = line.Scale.x * 0.5f;
        return local.x >= -halfLength && local.x <= halfLength &&
            local.z > -kLineTriggerRangeZ && local.z < kLineTriggerRangeZ;
    }
}

void LevelTriggerSystem::Initialize(const Player* player, const Stage* stage)
{
    m_player = player;
    m_stage = stage;
}

void LevelTriggerSystem::Update() const
{
    if (!m_player || !m_stage || m_player->GetHP() <= 0.0f) return;

    if (m_onCheckpointReached)
    {
        for (const auto& line : m_stage->m_linesCheckpoint)
        {
            // Re-read each iteration: a callback may move the player.
            if (IsOnLine(m_player->GetMovement()->GetPosition(), line))
            {
                m_onCheckpointReached(line.Position);
            }
        }
    }

    if (m_onEnableLineReached)
    {
        for (size_t i = 0; i < m_stage->m_linesEnable.size(); ++i)
        {
            if (IsOnLine(m_player->GetMovement()->GetPosition(), m_stage->m_linesEnable[i]))
            {
                m_onEnableLineReached(static_cast<int>(i));
            }
        }
    }
}