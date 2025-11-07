#include "Character.h"

// Character's movement, physics, and rendering primitives functions

void Character::UpdateVelocity(float elapsedTime)
{
    UpdateVerticalVelocity(elapsedTime);
    UpdateHorizontalVelocity(elapsedTime);
    UpdateVerticalMove(elapsedTime);
    UpdateHorizontalMove(elapsedTime);
}

void Character::Move(float elapsedTime, float vx, float vz, float speed)
{
    moveVecX = vx;
    moveVecZ = vz;
    maxMoveSpeed = speed;
}

void Character::Turn(float elapsedTime, float vx, float vz, float speed)
{
    speed *= elapsedTime;
    float n_vec = sqrtf((vx * vx) + (vz * vz));
    if (n_vec < 0.001f)
        return;

    vx /= n_vec;
    vz /= n_vec;

    float frontX = sinf(angle.y);
    float frontZ = cosf(angle.y);
    float cross = (vx * frontZ) - (vz * frontX);
    float dot = (vx * frontX) + (vz * frontZ);
    float rot = 1.0 - dot;

    if (rot > speed)
        rot = speed;

    if (cross < 0.0f)
        angle.y -= rot;
    else
        angle.y += rot;
}

void Character::Jump(float speed)
{
    velocity.y = speed;
}


void Character::UpdateVerticalVelocity(float elapsedTime)
{
    velocity.y += gravity * elapsedTime;
}

void Character::UpdateVerticalMove(float elapsedTime)
{
    position.y += velocity.y * elapsedTime;
    if (position.y < 0.0f)
    {
        position.y = 0.0f;
        velocity.y = 0.0f;

        isGround = true;
        if (isGround)
            OnLanding();
    }
    else
    {
        isGround = false;
    }
}

void Character::UpdateHorizontalVelocity(float elapsedTime)
{
    float lenght = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
    if (lenght > 0.0f)
    {
        float friction = this->friction * elapsedTime;
        if (position.y > 0)
            friction -= airControl * elapsedTime;

        if (lenght > friction)
        {
            float scale = (lenght - friction) / lenght;
            velocity.x *= scale;
            velocity.z *= scale;
        }
        else
        {
            velocity.x = 0.0f;
            velocity.z = 0.0f;
        }
    }

    if (lenght <= maxMoveSpeed)
    {
        float moveVecLenght = sqrtf(moveVecX * moveVecX + moveVecZ * moveVecZ);
        if (moveVecLenght > 0.0f)
        {
            float acceleration = this->acceleration * elapsedTime;
            if (position.y > 0)
                acceleration -= airControl * elapsedTime;

            velocity.x += (moveVecX / moveVecLenght) * acceleration;
            velocity.z += (moveVecZ / moveVecLenght) * acceleration;

            float lenght = sqrtf(moveVecX * moveVecX + moveVecZ * moveVecZ);
            if (lenght > maxMoveSpeed)
            {
                velocity.x = (moveVecX / lenght) * maxMoveSpeed;
                velocity.z = (moveVecZ / lenght) * maxMoveSpeed;
            }
        }
    }
    moveVecX = 0.0f;
    moveVecZ = 0.0f;
}

void Character::UpdateHorizontalMove(float elapsedTime)
{
    position.x += velocity.x * elapsedTime;
    position.z += velocity.z * elapsedTime;
}

void Character::UpdateTransform()
{
    DirectX::XMMATRIX S = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
    DirectX::XMMATRIX R = DirectX::XMMatrixRotationRollPitchYaw(angle.x, angle.y, angle.z);
    DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
    DirectX::XMMATRIX W = S * R * T;
    DirectX::XMStoreFloat4x4(&transform, W);
}

bool Character::ApplyDamage(int damage, float invincibleTime)
{
    if (damage == 0) return false;
    if (health <= 0) return false;
    if (invincibleTimer > 0) return false;

    invincibleTimer = invincibleTime;
    health -= damage;

    if (health <= 0)
        OnDead();
    else
        OnDamaged();

    return true;
}

void Character::AddImpulse(const DirectX::XMFLOAT3& impulse)
{
    velocity.x += impulse.x;
    velocity.y += impulse.y;
    velocity.z += impulse.z;
}

void Character::UpdateInvincibleTimer(float elapsedTime)
{
    if (invincibleTimer > 0.0f)
        invincibleTimer -= elapsedTime;
}


void Character::RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer)
{
    renderer->RenderCylinder(rc, position, radius, height, DirectX::XMFLOAT4(0, 0, 0, 1));
}