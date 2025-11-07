#pragma once

#include "System/ModelRenderer.h"
#include "Character.h"

//This is the abstract base class for enemy entities
class Enemy : public Character
{
public:
    Enemy() {}
    ~Enemy() override {}

    void Destroy();

    virtual void Update(float elapsedTime) = 0;
    virtual void Render(const RenderContext& rc, ModelRenderer* renderer) = 0;
};