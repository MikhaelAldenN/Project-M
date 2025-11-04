#pragma once

#include <vector>
#include <set>
#include "Projectile.h"

/// =============================================================
/// ProjectileManager
/// - Manages all projectiles (creation, update, removal, rendering)
/// =============================================================
class ProjectileManager
{
public:
    // =================================================================
    // Constructor/Destructor
    // =================================================================
    ProjectileManager();
    ~ProjectileManager();

    // =================================================================
    // Core Methods
    // =================================================================
    void Update(float elapsedTime);
    void Render(const RenderContext& rc, ModelRenderer* renderer);
    void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);

    void Register(Projectile* projectile);
    void Remove(Projectile* projectile);
    void Clear();

    // =================================================================
    // Accessors
    // =================================================================
    int GetProjectileCount() const { return static_cast<int>(projectiles.size()); }
    Projectile* GetProjectile(int index) { return projectiles.at(index); }

private:
    // =================================================================
    // Data Members
    // =================================================================
    std::vector<Projectile*> projectiles;
    std::set<Projectile*> removes;
};