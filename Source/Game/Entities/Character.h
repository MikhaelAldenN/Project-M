#pragma once
#include <DirectXMath.h>
#include <memory>
#include <string>
#include "System/ModelRenderer.h"
#include "System/ShapeRenderer.h"
#include "System/Model.h"
#include "CharacterMovement.h"

class Camera;

class Character
{
public:
    Character();
    virtual ~Character() = default;
    virtual void Update(float elapsedTime, Camera* camera) = 0;

    // Draws the model with the Lambert shader. Subclasses override to choose shader and color.
    virtual void Render(ModelRenderer* renderer);
    void RenderDebug(const RenderContext& rc, ShapeRenderer* renderer);

    DirectX::XMFLOAT3 GetPosition() const { return m_movement->GetPosition(); }

    // Returns raw pointer for read access — ownership stays here
    CharacterMovement* GetMovement() const { return m_movement.get(); }

    // Whether the character currently takes part in the game. What an inactive
    // character skips is decided by the subclass and by whoever owns it.
    [[nodiscard]] bool IsActive() const { return m_isActive; }
    void SetActive(bool active) { m_isActive = active; }

    DirectX::XMFLOAT3 m_scale = { 1.0f, 1.0f, 1.0f };

protected:
    // Syncs movement state to the visual model's root node
    void SyncData();

protected:
    std::unique_ptr<CharacterMovement> m_movement;
    std::shared_ptr<Model>             m_model;

private:
    bool m_isActive{ true };
};