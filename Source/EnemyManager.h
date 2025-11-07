#pragma once

#include <vector>
#include <set>
#include "Enemy.h"

/// EnemyManager
/// - Manages a collection of Enemy objects
/// - Handles registration, updates, rendering, and collisions
class EnemyManager
{
private:
    EnemyManager() {}
    ~EnemyManager() {}

    // Data Members
    std::vector<Enemy*> enemies;   // Active enemies
    std::set<Enemy*> removes;      // Enemies scheduled for removal

    // Internal Helper Functions
    void CollisionEnemyVsEnemies();

public:
    static EnemyManager& Instance()
    {
        static EnemyManager instance;
        return instance;
    }

    // Enemy Management Functions
    void Register(Enemy* enemy);      // Add a new enemy to the manager
    void Remove(Enemy* enemy);        // Mark an enemy for removal
    void Clear();                     // Remove all enemies from the manager

    // Update & Render Functions
    void Update(float elapsedTime);                                                 // Update all managed enemies
    void Render(const RenderContext& rc, ModelRenderer* renderer);                  // Render all enemies
    void RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer);    // Render debug primitives for enemies

    // Accessor Functions
    int GetEnemyCount() const { return static_cast<int>(enemies.size()); }  // Get the number of active enemies
    Enemy* GetEnemy(int index) { return enemies.at(index); }                // Get an enemy by index (with bounds checking)
};