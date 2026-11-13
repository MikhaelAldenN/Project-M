#include "AttackWall.h"
#include "CueBoss.h"
#include "WindowTrackingSystem.h"
#include "Player.h"
#include "System/Graphics.h"
#include "CameraController.h"
#include "Engine/Common/Constants.h"
#include <cassert>

using namespace DirectX;

namespace
{
    // Player-facing window title; moves to the text ID table in phase 2.5.
    constexpr const char* k_windowTitle{ "WALL.exe" };
    constexpr float k_blinkDimAlpha{ 0.25f };
}

AttackWall::AttackWall(const WallParams& params, const Player* target)
    : m_params(params), m_target(target) {}

void AttackWall::Start(CueBoss* boss, ProjectilePool* pool) {
    assert(pool && "AttackWall needs the phase's projectile pool");
    m_pool = pool;
    if (!m_mesh.IsLoaded()) m_mesh.Load("Data/Model/Character/PLACEHOLDER_mdl_Ball.glb");

    // Both ends keep the whole window on the monitor.
    const float halfWindowUnits{ m_params.windowWidth * 0.5f / Beyond::Config::PIXEL_TO_UNIT_RATIO };
    const float edgeX{ Beyond::Config::ARENA_WIDTH_UNITS * 0.5f - halfWindowUnits };

    // Why the far side: the player gets the full screen width to read the wall and dash.
    const float playerX{ m_target ? m_target->GetPosition().x : 1.0f };
    m_direction = (playerX >= 0.0f) ? 1.0f : -1.0f;
    m_wallX = -m_direction * edgeX;
    m_endX = m_direction * edgeX;

    m_stage = Stage::warning;
    m_stageTimer = 0.0f;
    // Why the address: unique while this attack lives, without touching any RNG.
    m_windowName = "wall_" + std::to_string(reinterpret_cast<std::uintptr_t>(this));

    if (!boss || !boss->GetWindowSystem()) return;

    TrackedWindowConfig cfg;
    cfg.name = m_windowName;
    cfg.title = k_windowTitle;
    cfg.width = static_cast<int>(m_params.windowWidth);
    cfg.height = Beyond::Config::CANVAS_HEIGHT;
    cfg.isTransparent = false;

    // Why capturing this: Stop removes the window before the attack is destroyed.
    boss->GetWindowSystem()->AddTrackedWindow(cfg,
        [this]() { return DirectX::XMFLOAT3{ m_wallX, 0.0f, 0.0f }; },
        [this]() { return DirectX::XMFLOAT2{ m_params.windowWidth, static_cast<float>(Beyond::Config::CANVAS_HEIGHT) }; }
    );
}

void AttackWall::Update(float dt, CueBoss* boss) {
    if (!boss || !boss->GetWindowSystem()) return;

    m_stageTimer += dt;

    switch (m_stage) {
    case Stage::warning:
        UpdateWarningBlink(boss);
        if (m_stageTimer >= m_params.warningDuration) {
            SetWindowAlpha(boss, 1.0f);
            SpawnColumn();
            CameraController::Instance().AddTrauma(0.3f);
            m_stage = Stage::sweep;
            m_stageTimer = 0.0f;
        }
        break;

    case Stage::sweep:
        MoveColumn(dt);
        // The sweep ends once the wall reaches the far edge.
        if ((m_wallX - m_endX) * m_direction >= 0.0f) {
            Finish(boss);
        }
        break;

    case Stage::done:
        break;
    }
}

void AttackWall::SpawnColumn() {
    m_column.clear();
    if (m_params.bulletCount <= 0) return;

    const float height{ Beyond::Config::ARENA_HEIGHT_UNITS };
    const float spacing{ height / static_cast<float>(m_params.bulletCount) };

    for (int i{ 0 }; i < m_params.bulletCount; ++i) {
        const ProjectileHandle handle{ m_pool->Spawn() };
        Projectile* projectile{ m_pool->Get(handle) };
        // Why: a full pool leaves a gap; the wall still sweeps and ends on schedule.
        if (!projectile) continue;

        projectile->position = { m_wallX, 0.0f, -height * 0.5f + spacing * (static_cast<float>(i) + 0.5f) };
        projectile->velocity = { m_direction * m_params.speed, 0.0f, 0.0f };
        projectile->radius = m_params.bulletRadius;
        projectile->visualScale = m_params.visualScale;
        projectile->damage = m_params.damage;
        m_column.push_back(handle);
    }
}

void AttackWall::MoveColumn(float dt) {
    m_wallX += m_direction * m_params.speed * dt;
    // Why clamped: the last frame must not carry the window past the screen edge.
    if ((m_wallX - m_endX) * m_direction > 0.0f) m_wallX = m_endX;

    // Why x is set, not advanced: the column stays exactly inside the window.
    for (const ProjectileHandle& handle : m_column) {
        if (Projectile * projectile{ m_pool->Get(handle) }) {
            projectile->position.x = m_wallX;
            projectile->age += dt;
        }
    }
}

void AttackWall::UpdateWarningBlink(CueBoss* boss) {
    const int halfCycle{ (m_params.blinkInterval > 0.0f)
        ? static_cast<int>(m_stageTimer / m_params.blinkInterval) : 0 };
    SetWindowAlpha(boss, (halfCycle % 2 == 0) ? 1.0f : k_blinkDimAlpha);
}

void AttackWall::SetWindowAlpha(CueBoss* boss, float alpha) {
    TrackedWindow* tracked{ boss->GetWindowSystem()->GetTrackedWindow(m_windowName) };
    if (tracked && tracked->window) {
        tracked->window->SetBackgroundAlpha(alpha);
    }
}

void AttackWall::DespawnColumn() {
    if (m_pool) {
        for (const ProjectileHandle& handle : m_column) {
            m_pool->Despawn(handle);
        }
    }
    m_column.clear();
}

void AttackWall::Finish(CueBoss* boss) {
    DespawnColumn();
    boss->GetWindowSystem()->RemoveTrackedWindow(m_windowName);
    m_stage = Stage::done;
}

void AttackWall::Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) {
    if (!m_pool || !m_mesh.IsLoaded()) return;
    auto modelRenderer = Graphics::Instance().GetModelRenderer();
    for (const ProjectileHandle& handle : m_column) {
        if (const Projectile * projectile{ m_pool->Get(handle) }) {
            m_mesh.Draw(*modelRenderer, ShaderId::Phong, { 1.0f, 0.2f, 0.6f, 1.0f },
                projectile->position, projectile->visualScale, 0.0f);
        }
    }
}

void AttackWall::Stop(CueBoss* boss) {
    if (boss && boss->GetWindowSystem()) {
        boss->GetWindowSystem()->RemoveTrackedWindow(m_windowName);
    }
    DespawnColumn();
    m_stage = Stage::done;
}

bool AttackWall::IsFinished() const {
    return m_stage == Stage::done;
}