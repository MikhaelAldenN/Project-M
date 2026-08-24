#pragma once

// Standard Libraries
#include <memory>
#include <string>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <PxPhysicsAPI.h>
#include <SDL3/SDL.h>
#include <imgui.h>
#include "PhysXUtils.h"
#include "Scene.h"
#include "System/Light.h"

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
// Tempat percobaan: stage kosong buat testing collision, mekanik baru,
// atau debug tooling, tanpa beban sistem SceneGame (dialog, boss, UI, FX).
// Isinya cuma Player + Stage + kamera top-down + collision.
//
// Cara pakai: ganti SANDBOX_STAGE_PATH di bawah kalau mau nyoba stage lain.
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
    void OnResize(int width, int height) override;

    Camera* GetMainCamera() const { return m_mainCamera.get(); }

private:
    struct Config {
        static constexpr float DEFAULT_SCREEN_W{ 1920.0f };
        static constexpr float DEFAULT_SCREEN_H{ 1080.0f };
        static constexpr float GRAVITY{ -9.81f };
        static constexpr float CAM_FOV{ 45.0f };
        static constexpr float CAM_NEAR{ 0.1f };
        static constexpr float CAM_FAR{ 1000.0f };
        static constexpr float CAM_START_HEIGHT{ 20.0f };
    };

    // Stage percobaan terpisah dari stage utama game.
    // Butuh referensi lain? Tinggal ganti path ini.
    static constexpr const char* SANDBOX_STAGE_PATH{ "Data/Model/Stage/ExampleStage.glb" };

    void RenderScene(float elapsedTime, Camera* camera);

    std::unique_ptr<Player> m_player{};
    std::unique_ptr<Stage> m_stage{};
    std::unique_ptr<CollisionManager> m_collisionManager{};
    std::shared_ptr<Camera> m_mainCamera{};

    LightManager m_lightManager{};

    DirectX::XMFLOAT3 m_cameraPosition{ 0.0f, 18.0f, 0.0f };
    DirectX::XMFLOAT3 m_cameraTarget{ 0.0f, 0.0f, 0.0f };
    const DirectX::XMFLOAT3 m_playerSpawnPos{ 0.0f, 2.0f, 0.0f };

    // =========================================================
    // PHYSX (sama minimal-nya kayak SceneBoss: gak butuh gravity berat,
    // tapi tetap disiapin karena Stage & Player butuh PxScene buat collision)
    // =========================================================
    physx::PxDefaultAllocator m_allocator{};
    physx::PxDefaultErrorCallback m_errorCallback{};
    std::unique_ptr<physx::PxFoundation, PhysXDeleter> m_foundation{};
    std::unique_ptr<physx::PxPhysics, PhysXDeleter> m_physics{};
    std::unique_ptr<physx::PxDefaultCpuDispatcher, PhysXDeleter> m_dispatcher{};
    std::unique_ptr<physx::PxScene, PhysXDeleter> m_scene{};
    std::unique_ptr<physx::PxControllerManager, PhysXDeleter> m_controllerManager{};
    std::unique_ptr<physx::PxMaterial, PhysXDeleter> m_defaultMaterial{};
    std::unique_ptr<physx::PxRigidStatic, PhysXDeleter> m_groundPlane{};

    // =========================================================
    // DEBUG / SANDBOX TOGGLES
    // =========================================================
    bool m_showGrid{ true };
    bool m_showStageDebug{ true };
    bool m_showPlayerInfo{ true };
};