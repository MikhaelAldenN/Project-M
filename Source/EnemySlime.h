#pragma once

#include "System/Model.h"
#include "Enemy.h"

/// =============================================================
/// EnemySlime
/// - Example enemy (slime); moves and renders slime model
/// =============================================================
class EnemySlime : public Enemy
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    EnemySlime();
    ~EnemySlime() override;

    // =================================================================
    // Core Methods
    // =================================================================
    void Update(float elapsedTime) override;
    void Render(const RenderContext& rc, ModelRenderer* renderer) override;

protected:
    // =================================================================
    // Event Handling
    // =================================================================
    void OnDead() override;

private:
    // =================================================================
    // Data Members
    // =================================================================
    Model* model = nullptr;
};