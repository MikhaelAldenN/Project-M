#include "Stage.h"

#include <chrono>

using namespace DirectX;

Stage::Stage(ID3D11Device* device, const std::string& modelPath)
{
    // Load the model (default: StageConfig::MODEL_PATH; callers may pass another)
    model = std::make_shared<Model>(device, modelPath.c_str());

    // Apply defaults
    position = StageConfig::DEFAULT_POS;
    rotation = StageConfig::DEFAULT_ROT;
    scale = StageConfig::DEFAULT_SCALE;
    color = StageConfig::DEFAULT_COLOR;

    // Initialize Debug Objects
    m_debugWalls.clear();
    for (const auto& originalWall : StageConfig::DEBUG_WALLS)
    {
        m_debugWalls.push_back(originalWall);
    }
    m_linesVoid = StageConfig::DEBUG_LINES_VOID;
    m_linesDisable = StageConfig::DEBUG_LINES_DISABLE;
    m_linesEnable = StageConfig::DEBUG_LINES_ENABLE;
    m_linesCheckpoint = StageConfig::DEBUG_LINES_CHECKPOINT;

    RebuildSpatialGrid();
    UpdateTransform();
    RebuildCollider();
}

void Stage::RebuildCollider()
{
    if (!model) return;

    // The collider reads each node's worldTransform, which UpdateTransform refreshes.
    UpdateTransform();

    // Each void line is placed relative to the stage's position and rotation, not its scale.
    const XMMATRIX stagePose{
        XMMatrixRotationRollPitchYaw(XMConvertToRadians(rotation.x), XMConvertToRadians(rotation.y), XMConvertToRadians(rotation.z))
        * XMMatrixTranslation(position.x, position.y, position.z) };

    std::vector<StageCollider::WallBox> walls{};
    walls.reserve(m_linesVoid.size());
    for (const DebugLineData& line : m_linesVoid)
    {
        const XMMATRIX linePose{
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(line.Rotation.x), XMConvertToRadians(line.Rotation.y), XMConvertToRadians(line.Rotation.z))
            * XMMatrixTranslation(line.Position.x, line.Position.y, line.Position.z) };

        StageCollider::WallBox wall{};
        XMStoreFloat4x4(&wall.world, linePose * stagePose);
        wall.halfExtents = { line.Scale.x * 0.5f, StageConfig::VOID_WALL_HALF_HEIGHT, StageConfig::VOID_WALL_HALF_DEPTH };
        walls.push_back(wall);
    }

    const auto start{ std::chrono::steady_clock::now() };
    m_collider.Build(*model, walls);
    const std::chrono::duration<float, std::milli> elapsed{ std::chrono::steady_clock::now() - start };
    m_colliderBuildMs = elapsed.count();
}

void Stage::RebuildSpatialGrid()
{
    m_spatialGrid.Clear();

    for (size_t i = 0; i < m_debugWalls.size(); ++i)
    {
        const auto& wall = m_debugWalls[i];

        // Calculate bounding radius (conservative estimate)
        float radiusX = wall.Scale.x;
        float radiusZ = wall.Scale.z;
        float boundingRadius = std::sqrt(radiusX * radiusX + radiusZ * radiusZ);

        // Insert into grid
        m_spatialGrid.Insert(wall.Position, boundingRadius, i);
    }
}

void Stage::UpdateTransform()
{
    if (!model) return;

    // Calculate World Matrix: Scale * Rotation * Translation
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotation.x),
        XMConvertToRadians(rotation.y),
        XMConvertToRadians(rotation.z)
    );
    XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);

    XMMATRIX world = S * R * T;

    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, world);

    // Apply to the model's root nodes
    model->UpdateTransform(worldFloat);
}

void Stage::Render(ModelRenderer* renderer)
{
    if (!model || !renderer) return;
    renderer->Draw(ShaderId::Phong, model, color);
}

void Stage::RenderDebug(ShapeRenderer* shapeRenderer, PrimitiveRenderer* primRenderer)
{
    if (shapeRenderer)
    {
        // Default Color (Green)
        DirectX::XMFLOAT4 defaultColor = { 0.0f, 1.0f, 0.0f, 1.0f };
        // Highlight Color (Yellow)
        DirectX::XMFLOAT4 highlightColor = { 1.0f, 1.0f, 0.0f, 1.0f };

        for (size_t i = 0; i < m_debugWalls.size(); ++i)
        {
            const auto& wall = m_debugWalls[i];
            DirectX::XMFLOAT4 drawColor = (m_highlightWallIndex == (int)i) ? highlightColor : defaultColor;
            DirectX::XMFLOAT3 rotRadians = {
                XMConvertToRadians(wall.Rotation.x),
                XMConvertToRadians(wall.Rotation.y),
                XMConvertToRadians(wall.Rotation.z)
            };

            // Use drawColor here
            shapeRenderer->DrawBox(wall.Position, rotRadians, wall.Scale, drawColor);
        }
    }

    // Draw Debug Lines
    if (primRenderer)
    {
        auto DrawLineList = [&](const std::vector<DebugLineData>& lines, DirectX::XMFLOAT4 defaultColor, DebugLineType listType)
            {
                for (int i = 0; i < lines.size(); ++i)
                {
                    const auto& line = lines[i];
                    DirectX::XMFLOAT4 finalColor = defaultColor;

                    if (m_highlightState.index == i && m_highlightState.type == listType)
                    {
                        finalColor = { 1.0f, 1.0f, 0.0f, 1.0f }; // Bright Yellow Highlight
                    }

                    XMMATRIX T = XMMatrixTranslation(line.Position.x, line.Position.y, line.Position.z);
                    XMMATRIX R = XMMatrixRotationRollPitchYaw(
                        XMConvertToRadians(line.Rotation.x),
                        XMConvertToRadians(line.Rotation.y),
                        XMConvertToRadians(line.Rotation.z)
                    );

                    float halfLen = line.Scale.x * 0.5f;
                    XMVECTOR p0 = XMVectorSet(-halfLen, 0, 0, 1);
                    XMVECTOR p1 = XMVectorSet(halfLen, 0, 0, 1);

                    XMMATRIX W = R * T;
                    p0 = XMVector3TransformCoord(p0, W);
                    p1 = XMVector3TransformCoord(p1, W);

                    XMFLOAT3 v0, v1;
                    XMStoreFloat3(&v0, p0);
                    XMStoreFloat3(&v1, p1);

                    primRenderer->AddVertex(v0, finalColor);
                    primRenderer->AddVertex(v1, finalColor);
                }
            };

        // Draw Lists with their Type identifier
        DrawLineList(m_linesVoid, { 0.0f, 1.0f, 1.0f, 1.0f }, DebugLineType::Void);         // Cyan
        DrawLineList(m_linesDisable, { 1.0f, 0.0f, 0.0f, 1.0f }, DebugLineType::Disable);   // Red
        DrawLineList(m_linesEnable, { 0.0f, 1.0f, 0.0f, 1.0f }, DebugLineType::Enable);     // Green
        DrawLineList(m_linesCheckpoint, { 0.2f, 0.4f, 1.0f, 1.0f }, DebugLineType::Checkpoint); // Bright Blue
    }
}

void Stage::AddDebugWall()
{
    DebugWallData newWall;

    if (!m_debugWalls.empty())
    {
        newWall = m_debugWalls.back();
    }
    else
    {
        // Fallback default if list is empty
        newWall.Position = { 0.0f, 0.0f, 0.0f };
        newWall.Rotation = { 0.0f, 0.0f, 0.0f };
        newWall.Scale = StageConfig::WALL_DEFAULT_SCALE;
    }

    m_debugWalls.push_back(newWall);
}

void Stage::AddDebugLine(DebugLineType type)
{
    DebugLineData newLine;
    newLine.Position = { 0,0,0 };
    newLine.Rotation = { 0,0,0 };
    newLine.Scale = StageConfig::LINE_DEFAULT_SCALE;

    // Determine which list to add to
    std::vector<DebugLineData>* targetList = nullptr;

    switch (type)
    {
    case DebugLineType::Void:       targetList = &m_linesVoid; break;
    case DebugLineType::Disable:    targetList = &m_linesDisable; break;
    case DebugLineType::Enable:     targetList = &m_linesEnable; break;
    case DebugLineType::Checkpoint: targetList = &m_linesCheckpoint; break;
    }

    if (targetList)
    {
        if (!targetList->empty()) newLine = targetList->back();
        targetList->push_back(newLine);
    }
}