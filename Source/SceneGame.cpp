#include "System/Graphics.h"
#include "System/Input.h"
#include "SceneGame.h"
#include "Camera.h"
#include "EnemyManager.h"
#include "EnemySlime.h"

void SceneGame::Initialize()
{
    stage = new Stage();
    player = new Player();

    Graphics& graphics = Graphics::Instance();
    Camera& camera = Camera::Instance();
    camera.SetLookAt(
        { 0, 10, -10 },
        { 0, 0, 0 },
        { 0, 1, 0 }
    );
    camera.SetPerspectiveFov(
        DirectX::XMConvertToRadians(45),
        graphics.GetScreenWidth() / graphics.GetScreenHeight(),
        0.1f,
        1000.0f
    );

    CameraController::Instance().SetControlMode(CameraControlMode::Mouse);

    EnemyManager& enemyManager = EnemyManager::Instance();
    for (int i = 0; i < 2; ++i) {
        EnemySlime* slime = new EnemySlime();
        slime->SetPosition(DirectX::XMFLOAT3(i * 2.0f, 0, 0));
        enemyManager.Register(slime);
    }
}

void SceneGame::Finalize()
{
    Input::Instance().GetMouse().LockCursor(false);

    if (stage != nullptr) {
        delete stage;
        stage = nullptr;
    }

    if (player != nullptr) {
        delete player;
        player = nullptr;
    }

    EnemyManager::Instance().Clear();
}

void SceneGame::Update(float elapsedTime)
{
    DirectX::XMFLOAT3 target = player->GetPosition();
    target.y += 0.5f;

    CameraController::Instance().SetTarget(target);
    CameraController::Instance().Update(elapsedTime);

    stage->Update(elapsedTime);
    player->Update(elapsedTime);

    EnemyManager::Instance().Update(elapsedTime);
}

void SceneGame::Render()
{
    Graphics& graphics = Graphics::Instance();
    ID3D11DeviceContext* dc = graphics.GetDeviceContext();
    ShapeRenderer* shapeRenderer = graphics.GetShapeRenderer();
    ModelRenderer* modelRenderer = graphics.GetModelRenderer();

    RenderContext rc;
    rc.deviceContext = dc;
    rc.lightDirection = { 0.0f, -1.0f, 0.0f };
    rc.renderState = graphics.GetRenderState();

    Camera& camera = Camera::Instance();
    rc.view = camera.GetView();
    rc.projection = camera.GetProjection();

    {
        stage->Render(rc, modelRenderer);
        player->Render(rc, modelRenderer);
        EnemyManager::Instance().Render(rc, modelRenderer);
    }

    {
        player->RenderDebugPrimitive(rc, shapeRenderer);
        EnemyManager::Instance().RenderDebugPrimitive(rc, shapeRenderer);
    }

    {
        // 2D sprite render (if any)
    }
}

void SceneGame::DrawGUI()
{
    player->DrawDebugGUI();
    CameraController::Instance().DrawDebugGUI();
}