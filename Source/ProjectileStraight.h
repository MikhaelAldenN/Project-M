#pragma once

#include "System/Model.h"
#include "Projectile.h"

/// =============================================================
/// ProjectileStraight
/// - Projectile that moves straight in a direction
/// =============================================================
class ProjectileStraight : public Projectile
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    ProjectileStraight(ProjectileManager* manager);
    ~ProjectileStraight() override;

    // =================================================================
    // Core Methods
    // =================================================================
    void Update(float elapsedTime) override;
    void Render(const RenderContext& rc, ModelRenderer* renderer) override;

    void Launch(const DirectX::XMFLOAT3& direction, const DirectX::XMFLOAT3& position);

private:
    // =================================================================
    // ProjectileStraight Data Members
    // =================================================================
    Model* model = nullptr;
    float speed = 10.0f;
    float lifeTimer = 3.0f;
};