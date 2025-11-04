#include "ProjectileManager.h"

// =================================================================
// ProjectileManager: Constructor/Destructor
// =================================================================
ProjectileManager::ProjectileManager()
{
}
ProjectileManager::~ProjectileManager()
{
    Clear();
}

// =================================================================
// Remove: Schedule projectile for removal
// =================================================================
void ProjectileManager::Remove(Projectile* projectile)
{
    removes.insert(projectile);
}

// =================================================================
// Update: Update all projectiles, safely remove marked ones
// =================================================================
void ProjectileManager::Update(float elapsedTime)
{
    // Use index-based loop to safely iterate when removing elements
    for (int i = static_cast<int>(projectiles.size()) - 1; i >= 0; --i)
    {
        if (projectiles[i] != nullptr)
        {
            projectiles[i]->Update(elapsedTime);
        }
    }

    for (Projectile* projectile : removes)
    {
        std::vector<Projectile*>::iterator it = std::find(projectiles.begin(), projectiles.end(), projectile);
        if (it != projectiles.end())
        {
            projectiles.erase(it);
        }
        delete projectile;
    }
    removes.clear();
}

// =================================================================
// Render: Render all projectiles
// =================================================================
void ProjectileManager::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    for (Projectile* projectile : projectiles)
    {
        if (projectile != nullptr)
        {
            projectile->Render(rc, renderer);
        }
    }
}

// =================================================================
// RenderDebugPrimitive: Render debug shapes for all projectiles
// =================================================================
void ProjectileManager::RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer)
{
    for (Projectile* projectile : projectiles)
    {
        if (projectile != nullptr)
        {
            projectile->RenderDebugPrimitive(rc, renderer);
        }
    }
}

// =================================================================
// Register: Add a new projectile to manager if not already present
// =================================================================
void ProjectileManager::Register(Projectile* projectile)
{
    if (projectile != nullptr &&
        std::find(projectiles.begin(), projectiles.end(), projectile) == projectiles.end())
    {
        projectiles.push_back(projectile);
    }
}

// =================================================================
// Clear: Delete all projectiles and clear lists
// =================================================================
void ProjectileManager::Clear()
{
    // Delete all projectiles
    for (Projectile* projectile : projectiles)
    {
        if (projectile != nullptr)
        {
            delete projectile;
        }
    }
    projectiles.clear();
}