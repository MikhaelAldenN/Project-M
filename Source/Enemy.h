#pragma once

#include "System/ModelRenderer.h"
#include "Character.h"

/// =============================================================
/// Enemy
/// - Abstract base class for enemy entities
/// =============================================================
class Enemy : public Character
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    Enemy() {}
    ~Enemy() override {}

    // =================================================================
    // Enemy Utility Methods
    // =================================================================
    void Destroy();

    // =================================================================
    // Pure Virtuals: Must implement in derived class
    // =================================================================
    virtual void Update(float elapsedTime) = 0;
    virtual void Render(const RenderContext& rc, ModelRenderer* renderer) = 0;
};