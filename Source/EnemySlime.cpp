#include "EnemySlime.h"

/// =============================================================
/// Constructor
/// - Loads slime model
/// - Sets default scale
/// =============================================================
EnemySlime::EnemySlime()
{
    model = new Model("Data/Model/Slime/Slime.mdl");
    scale.x = scale.y = scale.z = 0.01f;   // Small scale
}

/// =============================================================
/// Destructor
/// - Cleans up model memory
/// =============================================================
EnemySlime::~EnemySlime()
{
    delete model;
}

/// =============================================================
/// Update
/// - Updates movement, invincibility timer, and transform
/// - Also updates the model transform
/// =============================================================
void EnemySlime::Update(float elapsedTime)
{
    UpdateVelocity(elapsedTime);          // Movement logic
    UpdateInvincibleTimer(elapsedTime);   // Handle invincibility
    UpdateTransform();                    // Update world transform

    model->UpdateTransform();             // Sync model transform
}

/// =============================================================
/// OnDead
/// - Called when slime dies
/// - Destroys itself
/// =============================================================
void EnemySlime::OnDead()
{
    Destroy();
}

/// =============================================================
/// Render
/// - Renders slime with Lambert shader
/// =============================================================
void EnemySlime::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    renderer->Render(rc, transform, model, ShaderId::Lambert);
}
