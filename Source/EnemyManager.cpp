#include "EnemyManager.h"
#include "Collisions.h"

// Register a new enemy to be managed
void EnemyManager::Register(Enemy* enemy)
{
    enemies.emplace_back(enemy);
}

/// Update the state of all managed enemies
void EnemyManager::Update(float elapsedTime)
{
    // Update all enemies
    for (Enemy* enemy : enemies)
    {
        if (enemy)
        {
            enemy->Update(elapsedTime);
        }
    }

    // Handle pending enemy removals
    for (Enemy* enemy : removes)
    {
        enemies.erase(std::remove(enemies.begin(), enemies.end(), enemy), enemies.end());
        delete enemy;
    }

    // Clear removal list
    removes.clear();

    CollisionEnemyVsEnemies();
}

// Render all managed enemies
void EnemyManager::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    for (Enemy* enemy : enemies)
    {
        if (enemy)
        {
            enemy->Render(rc, renderer);
        }
    }
}

// Render debug primitives for all managed enemies
void EnemyManager::RenderDebugPrimitive(const RenderContext& rc, ShapeRenderer* renderer)
{
    for (Enemy* enemy : enemies)
    {
        if (enemy)
        {
            enemy->RenderDebugPrimitive(rc, renderer);
        }
    }
}

// Add an enemy to the removal list
void EnemyManager::Remove(Enemy* enemy)
{
    removes.insert(enemy);
}

// Delete all managed enemies and clear the list
void EnemyManager::Clear()
{
    for (Enemy* enemy : enemies)
    {
        delete enemy;
    }
    enemies.clear();
}

// Handle collision detection and response between enemies
// - Currently only adjusts enemy B's position
void EnemyManager::CollisionEnemyVsEnemies()
{
    int enemyCount = GetEnemyCount();

    // Check collisions between all unique pairs of enemies
    for (int i = 0; i < enemyCount - 1; ++i) {
        for (int j = i + 1; j < enemyCount; ++j) {
            Enemy* enemyA = GetEnemy(i);
            Enemy* enemyB = GetEnemy(j);

            // Collision check
            DirectX::XMFLOAT3 outPositionB;
            if (Collision::IntersectSphereVsSphere(
                enemyA->GetPosition(),   
                enemyA->GetRadius(),     
                enemyB->GetPosition(),   
                enemyB->GetRadius(),     
                outPositionB // Output: new pushed-back position for B
            )) {
                // Apply new position to enemy B
                enemyB->SetPosition(outPositionB);

                // (Optional) Could also calculate and apply push-back for enemy A
            }
        }
    }
}