#include "SceneSandbox.h"

#include "CameraController.h"
#include "Framework.h"
#include "System/CollisionManager.h"
#include "System/Graphics.h"
#include "InputHelper.h"

#include "Player.h"
#include "PlayerStates.h"
#include "Stage.h"

using namespace DirectX;

SceneSandbox::SceneSandbox()
{
    float screenW{ Config::DEFAULT_SCREEN_W };
    float screenH{ Config::DEFAULT_SCREEN_H };

    if (auto window{ Framework::Instance()->GetMainWindow() }) {
        screenW = static_cast<float>(window->GetWidth());
        screenH = static_cast<float>(window->GetHeight());
    }

    // --- Camera: top-down, sama style-nya SceneGame ---
    auto& camCtrl{ CameraController::Instance() };
    camCtrl.ClearCamera();
    camCtrl.StopSequence();
    camCtrl.SetTargetOffset({ 0.0f, 0.0f, 0.0f });
    camCtrl.SetFixedYawOffset(0.0f);
    camCtrl.SetFixedRollOffset(0.0f);

    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(Config::CAM_FOV), screenW / screenH, Config::CAM_NEAR, Config::CAM_FAR);

    XMFLOAT3 startPos{ m_cameraPosition };
    startPos.x = 0.0f;
    startPos.z = -14.0f;
    startPos.y = Config::CAM_START_HEIGHT;

    m_mainCamera->SetPosition(startPos);
    m_mainCamera->LookAt(m_cameraTarget);
    camCtrl.SetActiveCamera(m_mainCamera);
    camCtrl.SetControlMode(CameraControlMode::FixedFollow);
    camCtrl.SetFixedSetting(startPos);
    camCtrl.SetTarget(m_cameraTarget);

    // --- Stage: pakai model percobaan, bukan stage utama ---
    m_stage = std::make_unique<Stage>(Graphics::Instance().GetDevice(), SANDBOX_STAGE_PATH);

    // --- PhysX minimal setup (dibutuhkan Stage & Player buat collision) ---
    m_foundation.reset(PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback));
    assert(m_foundation != nullptr && "CRITICAL ERROR: PxCreateFoundation failed!");

    m_physics.reset(PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, physx::PxTolerancesScale(), true, nullptr));
    assert(m_physics != nullptr && "CRITICAL ERROR: PxCreatePhysics failed!");

    physx::PxSceneDesc sceneDesc(m_physics->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, Config::GRAVITY, 0.0f);

    m_dispatcher.reset(physx::PxDefaultCpuDispatcherCreate(2));
    sceneDesc.cpuDispatcher = m_dispatcher.get();
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;

    m_scene.reset(m_physics->createScene(sceneDesc));
    assert(m_scene != nullptr && "CRITICAL ERROR: createScene failed!");

    m_controllerManager.reset(PxCreateControllerManager(*m_scene));
    assert(m_controllerManager != nullptr && "CRITICAL ERROR: PxCreateControllerManager failed!");

    m_defaultMaterial.reset(m_physics->createMaterial(0.5f, 0.5f, 0.1f));
    assert(m_defaultMaterial != nullptr && "CRITICAL ERROR: createMaterial failed!");

    m_groundPlane.reset(physx::PxCreatePlane(*m_physics, physx::PxPlane(0, 1, 0, 0), *m_defaultMaterial));
    m_scene->addActor(*m_groundPlane);

    // --- Player ---
    m_player = std::make_unique<Player>();
    m_player->SetPosition(m_playerSpawnPos);
    m_player->InitPhysics(m_controllerManager.get(), m_defaultMaterial.get());
    m_stage->InitPhysics(m_physics.get(), m_scene.get(), m_defaultMaterial.get());
    m_player->SetMaxHP(100);

    PlayerConfig sandboxConfig{};
    sandboxConfig.moveSpeed = 8.0f;
    sandboxConfig.dashSpeed = 28.0f;
    m_player->ApplyConfig(sandboxConfig);
    m_player->GetMovement()->SetRotationY(DirectX::XM_PI);

    // --- Collision: cuma Player vs Stage, gak ada enemy/item manager ---
    // NOTE: kalau CollisionManager::Initialize gak nerima nullptr buat
    // enemyManager/itemManager, aku perlu lihat header-nya buat bikin overload.
    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), nullptr, nullptr);

    m_player->SetCollisionManager(m_collisionManager.get());
}

SceneSandbox::~SceneSandbox()
{
    m_player.reset();
    m_stage.reset();
    m_collisionManager.reset();
}

void SceneSandbox::Update(const float elapsedTime)
{
    if (m_scene) {
        m_scene->simulate(elapsedTime);
        m_scene->fetchResults(true);
    }

    Camera* activeCam{ CameraController::Instance().GetActiveCamera().get() };

    if (m_player) {
        m_player->Update(elapsedTime, activeCam);
        CameraController::Instance().SetTarget(m_player->GetPosition());
    }

    if (m_collisionManager) m_collisionManager->Update(elapsedTime);

    CameraController::Instance().Update(elapsedTime);
}

void SceneSandbox::Render(float elapsedTime, Camera* camera)
{
    Camera* targetCam{ camera ? camera : m_mainCamera.get() };
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto rs{ Graphics::Instance().GetRenderState() };

    // Simple clear, gak pakai post-process pipeline (biar ringan buat testing)
    ID3D11RenderTargetView* originalRTV{ nullptr };
    ID3D11DepthStencilView* originalDSV{ nullptr };
    dc->OMGetRenderTargets(1, &originalRTV, &originalDSV);
    if (originalRTV) {
        float clearColor[4]{ 0.05f, 0.05f, 0.08f, 1.0f };
        dc->ClearRenderTargetView(originalRTV, clearColor);
        originalRTV->Release();
    }
    if (originalDSV) {
        dc->ClearDepthStencilView(originalDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
        originalDSV->Release();
    }

    dc->OMSetBlendState(rs->GetBlendState(BlendState::Opaque), nullptr, 0xFFFFFFFF);
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
    dc->RSSetState(rs->GetRasterizerState(RasterizerState::SolidCullBack));

    RenderScene(elapsedTime, targetCam);

    if (targetCam == m_mainCamera.get()) {
        auto shapeRenderer{ Graphics::Instance().GetShapeRenderer() };
        auto primRenderer{ Graphics::Instance().GetPrimitiveRenderer() };

        if (m_showGrid) primRenderer->DrawGrid(50, 1.0f);
        if (m_showStageDebug && m_stage) m_stage->RenderDebug(shapeRenderer, primRenderer);

        shapeRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection());
        primRenderer->Render(dc, targetCam->GetView(), targetCam->GetProjection(), D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    DrawGUI();
}

void SceneSandbox::RenderScene(const float elapsedTime, Camera* camera)
{
    if (!camera) return;
    auto dc{ Graphics::Instance().GetDeviceContext() };
    auto modelRenderer{ Graphics::Instance().GetModelRenderer() };
    RenderContext rc{ dc, Graphics::Instance().GetRenderState(), camera, &m_lightManager };

    if (m_player)
    {
        modelRenderer->Draw(ShaderId::Phong, m_player->GetModel(), m_player->color);
        m_player->RenderWeapon(modelRenderer);
        m_player->RenderProjectiles(modelRenderer);
    }
    if (m_stage)
    {
        m_stage->UpdateTransform();
        m_stage->Render(modelRenderer);
    }

    modelRenderer->Render(rc);
}

void SceneSandbox::DrawGUI()
{
    ImGui::Begin("Sandbox");

    ImGui::TextDisabled("Stage: %s", SANDBOX_STAGE_PATH);
    ImGui::Separator();

    ImGui::Checkbox("Show Grid", &m_showGrid);
    ImGui::Checkbox("Show Stage Debug (walls/lines)", &m_showStageDebug);
    ImGui::Checkbox("Show Player Info", &m_showPlayerInfo);

    if (m_showPlayerInfo && m_player)
    {
        const auto pos{ m_player->GetPosition() };
        ImGui::Separator();
        ImGui::Text("Player Pos: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
        ImGui::Text("Player HP: %.1f", m_player->GetHP());
    }

    ImGui::End();
}

void SceneSandbox::OnResize(int width, int height)
{
    if (height <= 0) height = 1;
    if (m_mainCamera) {
        m_mainCamera->SetPerspectiveFov(DirectX::XMConvertToRadians(Config::CAM_FOV), static_cast<float>(width) / static_cast<float>(height), Config::CAM_NEAR, Config::CAM_FAR);
    }
}