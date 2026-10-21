#pragma once

// Standard Libraries
#include <memory>
#include <string>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include "Scene.h"
#include "System/Light.h"
#include "DebugUI.h"

// ==========================================
// FORWARD DECLARATIONS
// ==========================================
class Camera;
class CollisionManager;
class Player;
class Stage;

// ==========================================
// SCENE SANDBOX
// ==========================================
// Test scene: an empty stage for trying collision, new mechanics
// or debug tooling without the SceneGame systems (dialogue, boss, UI, FX).
// Contains only the player, the stage, a top-down camera and collision.
//
// To try another stage, change SANDBOX_STAGE_PATH below.
// ==========================================
class SceneSandbox : public Scene
{
public:
    SceneSandbox();
    ~SceneSandbox() override;

    SceneSandbox(const SceneSandbox&) = delete;
    SceneSandbox& operator=(const SceneSandbox&) = delete;

    void Update(float elapsedTime) override;
    void Render(float elapsedTime, Camera* camera = nullptr) override;
    void DrawGUI() override;
    [[nodiscard]] Kind GetKind() const override { return Kind::sandbox; }

    Camera* GetMainCamera() const { return m_mainCamera.get(); }

private:
    struct Config {
        static constexpr float CAM_FOV{ 45.0f };
        static constexpr float CAM_NEAR{ 0.1f };
        static constexpr float CAM_FAR{ 1000.0f };
        static constexpr float CAM_START_HEIGHT{ 20.0f };
    };

    // Test stage, separate from the main game stage.
    static constexpr const char* SANDBOX_STAGE_PATH{ "Data/Model/Stage/ExampleStage.glb" };

    void RenderScene(float elapsedTime, Camera* camera);

    void DrawDebugPanel();

    std::unique_ptr<Player> m_player{};
    std::unique_ptr<Stage> m_stage{};
    std::unique_ptr<CollisionManager> m_collisionManager{};
    std::shared_ptr<Camera> m_mainCamera{};

    LightManager m_lightManager{};

    DirectX::XMFLOAT3 m_cameraPosition{ 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 m_cameraTarget{ 0.0f, 0.0f, 0.0f };
    const DirectX::XMFLOAT3 m_playerSpawnPos{ 0.0f, 2.0f, 0.0f };

    // =========================================================
    // DEBUG / SANDBOX TOGGLES
    // =========================================================
    bool m_showGrid{ true };
    bool m_showStageDebug{ true };
    bool m_showPlayerInfo{ true };

#if defined(_DEBUG)
    // Keeps the "Sandbox" tab registered while this scene lives.
    // Declared last: destroyed first, before any member the callback reads.
    DebugPanelHandle m_debugPanel{};
    DebugPanelHandle m_playerPanel{};
#endif

};