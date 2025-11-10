#include <imgui.h>
#include "Player.h"
#include "Camera.h"
#include "CameraController.h"
#include "System/Input.h"
#include "EnemyManager.h"
#include "Collisions.h"
#include "ProjectileStraight.h"
#include "PlayerState_Idle.h"

//TODO: Come back here for refactoring after implementing more states

Player::Player()
{
    model = new Model("Data/Model/Player/AnimationTest.mdl");
    scale.x = scale.y = scale.z = 0.01f;
    animation = new Animation();
    stateMachine = new StateMachine();
    stateMachine->ChangeState(new PlayerState_Idle(this));
}
Player::~Player()
{
    delete model;
    delete stateMachine;
    delete animation;
}

//Core Update
void Player::Update(float elapsedTime)
{
    InputMove(elapsedTime);
    InputJump();
    InputProjectile();

    CollisionPlayerVsEnemies();
    CollisionProjectilesVsEnemies();

    stateMachine->Update(elapsedTime);
    animation->UpdateAnimation(model, elapsedTime);

    UpdateTransform();
    model->UpdateTransform();
    UpdateVelocity(elapsedTime);

    projectileManager.Update(elapsedTime);
}

bool Player::IsRunning() const
{
    DirectX::XMFLOAT3 moveVec = GetMoveVec();
    return fabs(moveVec.x) > 0.01f || fabs(moveVec.z) > 0.01f;
}

bool Player::IsSprinting() const
{
    // Cek apakah sedang bergerak DAN tombol SHIFT ditekan.
    // Kamu bisa tambahkan input gamepad di sini juga nanti.
    bool isShiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    return IsRunning() && isShiftPressed;
}

void Player::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    renderer->Render(rc, transform, model, ShaderId::Lambert);
    projectileManager.Render(rc, renderer);
}

void Player::InputMove(float elapsedTime)
{
    DirectX::XMFLOAT3 moveVec = GetMoveVec();
    Move(elapsedTime, moveVec.x, moveVec.z, moveSpeed);
    Turn(elapsedTime, moveVec.x, moveVec.z, turnSpeed);
}
void Player::InputJump()
{
    GamePad& gamePad = Input::Instance().GetGamePad();
    if (gamePad.GetButtonDown() & GamePad::BTN_A)
        Jump(jumpSpeed);
}
void Player::InputProjectile()
{
    GamePad& gamePad = Input::Instance().GetGamePad();
    if (gamePad.GetButtonDown() & GamePad::BTN_X)
    {
        DirectX::XMFLOAT3 dir;
        dir.x = sinf(angle.y);
        dir.y = 0.0f;
        dir.z = cosf(angle.y);

        DirectX::XMFLOAT3 pos;
        pos.x = this->position.x;
        pos.y = this->position.y + 1.0f;
        pos.z = this->position.z;

        ProjectileStraight* projectile = new ProjectileStraight(&projectileManager);
        projectile->Launch(dir, pos);
    }
}

void Player::Jump(float speed)
{
    velocity.y += speed;
}

void Player::CollisionPlayerVsEnemies()
{
    EnemyManager& enemyManager = EnemyManager::Instance();
    int enemyCount = enemyManager.GetEnemyCount();

    for (int i = 0; i < enemyCount; ++i)
    {
        Enemy* enemy = enemyManager.GetEnemy(i);

        DirectX::XMFLOAT3 outPosition;
        if (Collision::IntersectSphereVsSphere(
            this->GetPosition(), this->GetRadius(),
            enemy->GetPosition(), enemy->GetRadius(),
            outPosition
        ))
        {
            enemy->SetPosition(outPosition);

            DirectX::XMVECTOR posA = DirectX::XMLoadFloat3(&this->GetPosition());
            DirectX::XMVECTOR posB = DirectX::XMLoadFloat3(&outPosition);
            DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(posA, posB);
            DirectX::XMVECTOR vecNormalized = DirectX::XMVector3Normalize(vec);
            DirectX::XMVECTOR newPosA = DirectX::XMVectorMultiplyAdd(
                vecNormalized,
                DirectX::XMVectorReplicate(this->GetRadius() + enemy->GetRadius()),
                posB
            );
            DirectX::XMStoreFloat3(&this->position, newPosA);
        }
    }
}

void Player::CollisionProjectilesVsEnemies()
{
    EnemyManager& enemyManager = EnemyManager::Instance();
    int projectileCount = projectileManager.GetProjectileCount();
    int enemyCount = enemyManager.GetEnemyCount();

    for (int i = 0; i < projectileCount; ++i)
    {
        Projectile* projectile = projectileManager.GetProjectile(i);
        for (int j = 0; j < enemyCount; ++j)
        {
            Enemy* enemy = enemyManager.GetEnemy(j);

            DirectX::XMFLOAT3 outPosition;
            if (Collision::IntersectSphereVsCylinder(
                projectile->GetPosition(),
                projectile->GetRadius(),
                enemy->GetPosition(),
                enemy->GetRadius(),
                enemy->GetHeight(),
                outPosition))
            {
                if (enemy->ApplyDamage(1, 0.5f))
                {
                    DirectX::XMFLOAT3 impulse;
                    const float power = 10.0f;
                    const DirectX::XMFLOAT3& e = enemy->GetPosition();
                    const DirectX::XMFLOAT3& p = projectile->GetPosition();

                    float x = e.x - p.x;
                    float y = e.y - p.y;
                    float z = e.z - p.z;
                    float length = sqrtf(x * x + y * y + z * z);
                    x /= length; y /= length; z /= length;

                    impulse.x = x * power;
                    impulse.y = power * 0.5f;
                    impulse.z = z * power;

                    enemy->AddImpulse(impulse);
                    projectile->Destroy();
                }
            }
        }
    }
}

void Player::DrawDebugGUI()
{
    ImVec2 pos = ImGui::GetMainViewport()->GetWorkPos();
    ImGui::SetNextWindowPos(ImVec2(pos.x + 10, pos.y + 10), ImGuiCond_Once);
    ImGui::SetWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Player", nullptr, ImGuiWindowFlags_None))
    {
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::InputFloat3("Position", &position.x);

            DirectX::XMFLOAT3 a;
            a.x = DirectX::XMConvertToDegrees(angle.x);
            a.y = DirectX::XMConvertToDegrees(angle.y);
            a.z = DirectX::XMConvertToDegrees(angle.z);
            ImGui::InputFloat3("Angle", &a.x);
            angle.x = DirectX::XMConvertToRadians(a.x);
            angle.y = DirectX::XMConvertToRadians(a.y);
            angle.z = DirectX::XMConvertToRadians(a.z);

            ImGui::InputFloat3("Scale", &scale.x);
        }

        // === Tambahkan ini: tampilkan FSM state ===
        const char* stateName = "Unknown";
        if (stateMachine && stateMachine->GetCurrentState())
            stateName = stateMachine->GetCurrentState()->GetName();

        float currentSpeed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

        ImGui::Text("Total Horizontal Speed: %.2f", currentSpeed);

        //ImGui::Text("SPEED: %.2f", this->velocity.x);
        ImGui::Text("FSM State: %s", stateName);

        //if (ImGui::CollapsingHeader("Animation Debug", ImGuiTreeNodeFlags_DefaultOpen))
        //{
        //    ImGui::Text("Is Playing: %s", animation->IsPlaying() ? "TRUE" : "FALSE");
        //    ImGui::Text("Time: %.2f", animation->GetCurrentTime());
        //    ImGui::Text("Index: %d", animation->GetCurrentIndex());
        //}
    }
    ImGui::End();
}
void Player::RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer)
{
    Character::RenderDebugPrimitive(rc, renderer);
    projectileManager.RenderDebugPrimitive(rc, renderer);
}

//Helper for movement vector
DirectX::XMFLOAT3 Player::GetMoveVec() const
{
    GamePad& gamePad = Input::Instance().GetGamePad();
    float ax = gamePad.GetAxisLX();
    float ay = gamePad.GetAxisLY();

    DirectX::XMFLOAT3 vec;
    CameraControlMode currentMode = CameraController::Instance().GetControlMode();

    if (currentMode == CameraControlMode::FixedFollow)
    {
        vec.x = ax;
        vec.z = ay;
        vec.y = 0.0f;
    }
    else
    {
        Camera& camera = Camera::Instance();
        const DirectX::XMFLOAT3& cameraRight = camera.GetRight();
        const DirectX::XMFLOAT3& cameraFront = camera.GetFront();

        float cameraRightX = cameraRight.x;
        float cameraRightZ = cameraRight.z;
        float cameraRightLength = sqrtf(cameraRightX * cameraRightX + cameraRightZ * cameraRightZ);
        if (cameraRightLength > 0.0f) {
            cameraRightX /= cameraRightLength;
            cameraRightZ /= cameraRightLength;
        }

        float cameraFrontX = cameraFront.x;
        float cameraFrontZ = cameraFront.z;
        float cameraFrontLength = sqrtf(cameraFrontX * cameraFrontX + cameraFrontZ * cameraFrontZ);
        if (cameraFrontLength > 0.0f) {
            cameraFrontX /= cameraFrontLength;
            cameraFrontZ /= cameraFrontLength;
        }

        vec.x = cameraRightX * ax + cameraFrontX * ay;
        vec.z = cameraRightZ * ax + cameraFrontZ * ay;
        vec.y = 0.0f;
    }

    float length = sqrtf(vec.x * vec.x + vec.z * vec.z);
    if (length > 0.0f) {
        vec.x /= length;
        vec.z /= length;
    }
    return vec;
}