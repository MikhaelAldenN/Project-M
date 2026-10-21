#include "SceneSandbox.h"

#include "CameraController.h"
#include "Framework.h"
#include "Game/Collision/CollisionManager.h"
#include "System/Graphics.h"
#include "InputHelper.h"
#include "Engine/Common/Constants.h"

#include "Player.h"
#include "PlayerStates.h"
#include "Stage.h"

using namespace DirectX;

SceneSandbox::SceneSandbox()
{
    constexpr float canvasAspect{ static_cast<float>(Beyond::Config::CANVAS_WIDTH)
           / static_cast<float>(Beyond::Config::CANVAS_HEIGHT) };

    // --- Camera: top-down, same style as SceneGame ---
    auto& camCtrl{ CameraController::Instance() };
    camCtrl.ClearCamera();
    camCtrl.StopSequence();
    camCtrl.SetTargetOffset({ 0.0f, 0.0f, 0.0f });
    camCtrl.SetFixedYawOffset(0.0f);
    camCtrl.SetFixedRollOffset(0.0f);

    m_mainCamera = std::make_shared<Camera>();
    m_mainCamera->SetPerspectiveFov(XMConvertToRadians(Config::CAM_FOV), canvasAspect, Config::CAM_NEAR, Config::CAM_FAR);

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

    // --- Stage: a test model, not the main stage ---
    m_stage = std::make_unique<Stage>(Graphics::Instance().GetDevice(), SANDBOX_STAGE_PATH);
    
    // --- Clear unwanted features for SceneSandbox ---
    m_stage->m_debugWalls.clear();
    m_stage->m_linesVoid.clear();
    m_stage->m_linesDisable.clear();
    m_stage->m_linesEnable.clear();
    m_stage->m_linesCheckpoint.clear();
    m_stage->RebuildCollider();

    // --- Player ---
    m_player = std::make_unique<Player>();
    m_player->SetPosition(m_playerSpawnPos);
    m_player->SetStageCollider(&m_stage->GetCollider());
    m_player->SetMaxHP(100);

    PlayerConfig sandboxConfig{};
    sandboxConfig.moveSpeed = 8.0f;
    sandboxConfig.dashSpeed = 28.0f;
    m_player->ApplyConfig(sandboxConfig);
    m_player->GetMovement()->SetRotationY(DirectX::XM_PI);

    // --- Collision: player against the stage only; no enemy or item manager ---
    m_collisionManager = std::make_unique<CollisionManager>();
    m_collisionManager->Initialize(m_player.get(), m_stage.get(), nullptr, nullptr);

    m_player->SetCollisionManager(m_collisionManager.get());

#if defined(_DEBUG)
    // Capturing `this` is safe: the handles are members and die with this scene.
    m_debugPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Sandbox", [this]() { DrawDebugPanel(); });
    m_playerPanel = DebugUI::Instance().RegisterPanel(DebugPanelSlot::tab, "Player", [this]() {
        if (m_player) m_player->DrawDebugGUI();
        });
#endif
}

SceneSandbox::~SceneSandbox()
{
    m_player.reset();
    m_stage.reset();
    m_collisionManager.reset();
}

void SceneSandbox::Update(const float elapsedTime)
{
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

    // Plain clear without the post-process pipeline, to keep testing light
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

#if !defined(_DEBUG)
    // Release keeps the floating window until ImGui is removed (beta).
    DrawGUI();
#endif

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
    DrawDebugPanel();
    ImGui::End();
}

void SceneSandbox::DrawDebugPanel()
{
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

    if (m_stage)
    {
        StageCollider& collider{ m_stage->GetCollider() };
        ImGui::Separator();
        ImGui::Text("Collider: %zu triangles, built in %.1f ms",
            collider.GetTriangleCount(), m_stage->GetColliderBuildMs());

        if (m_player)
        {
            // Probe: the downward cast should report the floor under the player's feet.
            // The upward cast reaches the floor from below and shows whether back faces are hit.
            constexpr float probeRadius{ 0.5f };
            constexpr float probeHeight{ 3.0f };
            const XMFLOAT3 feet{ m_player->GetPosition() };
            StageCollider::SweepHit hit{};

            const XMFLOAT3 above{ feet.x, feet.y + probeHeight, feet.z };
            if (collider.SphereCast(above, { 0.0f, -1.0f, 0.0f }, probeRadius, probeHeight * 2.0f, hit))
            {
                ImGui::Text("Down: dist %.3f  floor %.3f  contact Y %.3f  n (%.2f, %.2f, %.2f)",
                    hit.distance, above.y - hit.distance - probeRadius, hit.position.y,
                    hit.normal.x, hit.normal.y, hit.normal.z);
            }
            else
            {
                ImGui::TextUnformatted("Down: no hit");
            }

            const XMFLOAT3 below{ feet.x, feet.y - probeHeight, feet.z };
            if (collider.SphereCast(below, { 0.0f, 1.0f, 0.0f }, probeRadius, probeHeight * 2.0f, hit))
            {
                ImGui::Text("Up: dist %.3f  contact Y %.3f  n (%.2f, %.2f, %.2f)",
                    hit.distance, hit.position.y, hit.normal.x, hit.normal.y, hit.normal.z);
            }
            else
            {
                ImGui::TextUnformatted("Up: no hit");
            }
        }
    }
}