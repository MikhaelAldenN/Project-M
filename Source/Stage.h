#pragma once

#include "System/ModelRenderer.h"

/// =============================================================
/// Stage
/// - Represents the static stage/environment model
/// =============================================================
class Stage
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    Stage();
    ~Stage();

    // =================================================================
    // Main Logic
    // =================================================================
    void Update(float elapsedTime);
    void Render(const RenderContext& rc, ModelRenderer* renderer);

private:
    // =================================================================
    // Stage Data Members
    // =================================================================
    Model* model = nullptr;
};