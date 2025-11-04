#pragma once

#include "System/ModelRenderer.h"
#include "System/ShapeRenderer.h"

class ProjectileManager;

/// =============================================================
/// Projectile (Abstract Base Class)
/// - Base for all projectile types (e.g., straight, homing)
/// - Handles transform, collision, and rendering
/// =============================================================
class Projectile
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    Projectile(ProjectileManager* manager);
    virtual ~Projectile() {}

    // =================================================================
    // Update & Render (Pure Virtuals)
    // =================================================================
    virtual void Update(float elapsedTime) = 0;
    virtual void Render(const RenderContext& rc, ModelRenderer* renderer) = 0;

    // =================================================================
    // Utility Methods
    // =================================================================
    void Destroy();
    virtual void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);

    // =================================================================
    // Accessors
    // =================================================================
    const DirectX::XMFLOAT3& GetPosition() const { return position; }
    const DirectX::XMFLOAT3& GetDirection() const { return direction; }
    const DirectX::XMFLOAT3& GetScale() const { return scale; }
    float GetRadius() const { return radius; }

protected:
    // =================================================================
    // Internal Transform Update
    // =================================================================
    void UpdateTransform();
    ProjectileManager* manager = nullptr;

protected:
    // =================================================================
    // Projectile Data Members
    // =================================================================
    DirectX::XMFLOAT3 position = { 0, 0, 0 };
    DirectX::XMFLOAT3 direction = { 0, 0, 1 };
    DirectX::XMFLOAT3 scale = { 1, 1, 1 };
    DirectX::XMFLOAT4X4 transform = { 1, 0, 0, 0,
                                      0, 1, 0, 0,
                                      0, 0, 1, 0,
                                      0, 0, 0, 1 };

    float radius = 0.5f; // Default radius for collision detection
};