#include "Projectile.h"
#include "ProjectileManager.h"

// =================================================================
// Projectile: Constructor - Registers itself with manager
// =================================================================
Projectile::Projectile(ProjectileManager* manager) : manager(manager)
{
    manager->Register(this);
}

// =================================================================
// UpdateTransform: Builds world transform matrix for projectile
// =================================================================
void Projectile::UpdateTransform()
{
    DirectX::XMVECTOR Front, Up, Right;
    Front = DirectX::XMLoadFloat3(&direction); // Front vector
    Front = DirectX::XMVector3Normalize(Front);

    Up = DirectX::XMVectorSet(0.001f, 1.0f, 0.0f, 0.0f); // Temporary up vector
    Up = DirectX::XMVector3Normalize(Up);

    Right = DirectX::XMVector3Cross(Up, Front); // Right vector
    Right = DirectX::XMVector3Normalize(Right);

    Up = DirectX::XMVector3Cross(Front, Right); // Correct up vector

    DirectX::XMFLOAT3 right, up, front;
    DirectX::XMStoreFloat3(&right, Right);
    DirectX::XMStoreFloat3(&up, Up);
    DirectX::XMStoreFloat3(&front, Front);

    // Build transform matrix
    transform._11 = right.x * scale.x;
    transform._12 = right.y * scale.x;
    transform._13 = right.z * scale.x;
    transform._14 = 0.0f;

    transform._21 = up.x * scale.y;
    transform._22 = up.y * scale.y;
    transform._23 = up.z * scale.y;
    transform._24 = 0.0f;

    transform._31 = front.x * scale.z;
    transform._32 = front.y * scale.z;
    transform._33 = front.z * scale.z;
    transform._34 = 0.0f;

    transform._41 = position.x;
    transform._42 = position.y;
    transform._43 = position.z;
    transform._44 = 1.0f;

    this->direction = front;
}

// =================================================================
// Destroy: Remove projectile from manager
// =================================================================
void Projectile::Destroy()
{
    manager->Remove(this);
}

// =================================================================
// RenderDebugPrimitive: Render projectile debug shape (sphere)
// =================================================================
void Projectile::RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer)
{
    renderer->RenderSphere(rc, position, radius, DirectX::XMFLOAT4(0, 0, 1, 1));
}