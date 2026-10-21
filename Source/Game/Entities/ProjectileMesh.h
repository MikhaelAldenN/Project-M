#pragma once

#include <memory>
#include <DirectXMath.h>
#include "System/Model.h"
#include "System/ModelRenderer.h"

// One projectile model, loaded once and drawn at many places with a world matrix per draw.
class ProjectileMesh
{
public:
    // Loads the model at `path`. Every mesh of the model must hang off the same node.
    void Load(const char* path);
    [[nodiscard]] bool IsLoaded() const { return m_model != nullptr; }

    // Queues one draw at `position`, scaled uniformly by `scale` and turned `yawRadians` about Y.
    void Draw(ModelRenderer& renderer, ShaderId shader, const DirectX::XMFLOAT4& color,
        const DirectX::XMFLOAT3& position, float scale, float yawRadians) const;

private:
    std::shared_ptr<Model> m_model;
    DirectX::XMFLOAT4X4 m_meshOffset{}; // mesh node transform relative to the model root
};