#include "CollisionManager.h"
#include "CueBoss.h"
#include "CollisionGeometry.h"
#include "ProjectilePool.h"
#include "EffectManager.h"
#include "TimeManager.h"
#include <CameraController.h>

using namespace DirectX;
using CollisionGeometry::AABB;
using CollisionGeometry::CheckAABBIntersection;
using CollisionGeometry::CreateSweptAABB;
using CollisionGeometry::DistancePointToLineSegment2D;
using CollisionGeometry::RayCastOBB;

// =========================================================
// INITIALIZATION OVERLOADS
// =========================================================

void CollisionManager::Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im)
{
    m_player = p;
    m_stage = s;
    m_enemyManager = em;
    m_itemManager = im;
}

// =========================================================
// UPDATE
// =========================================================

void CollisionManager::Update(float elapsedTime)
{
    CheckEnemyProjectilesFull(elapsedTime);
    CheckPlayerProjectilesVsEnemies(elapsedTime);
    CheckPlayerProjectilesVsCue(elapsedTime);
    CheckCueAllyProjectilesVsPlayer(elapsedTime);
    CheckPlayerVsEnemies();

    if (m_itemManager)
    {
        CheckPlayerVsItems();
    }
    
    CheckCueProjectilesVsEnemies(elapsedTime);
    CheckBossProjectilesVsPlayer(elapsedTime);
    CheckPlayerProjectilesVsBoss();
}

void CollisionManager::CheckPlayerProjectilesVsBoss()
{
    if (!m_Boss || !m_player) return;

    ICuePhase* phase{ m_Boss->GetCurrentPhase() };
    if (!phase || !phase->IsVulnerable()) return;

    // The boss window is 5 x 5 world units, so half of that on each axis.
    constexpr float k_halfWidth{ 2.5f };
    constexpr float k_halfDepth{ 2.5f };
    const DirectX::XMFLOAT3 bossPos{ m_Boss->GetPosition() };

    ProjectilePool& projectiles{ m_player->GetProjectiles() };
    const std::vector<Projectile>& slots{ projectiles.Slots() };
    for (std::size_t i{ 0 }; i < slots.size(); ++i) {
        if (!slots[i].isActive) continue;

        const DirectX::XMFLOAT3 bPos{ slots[i].position };
        if (bPos.x > (bossPos.x - k_halfWidth) && bPos.x < (bossPos.x + k_halfWidth) &&
            bPos.z >(bossPos.z - k_halfDepth) && bPos.z < (bossPos.z + k_halfDepth))
        {
            const int damage{ slots[i].damage };
            projectiles.Despawn(i);
            phase->TakeDamage(DamageInfo{ damage, bPos });
        }
    }
}

void CollisionManager::CheckEnemyProjectilesFull(float elapsedTime) 
{
    if (!m_enemyManager) return;

    for (auto& enemy : m_enemyManager->GetEnemies())
    {
        ProjectilePool& projectiles{ enemy->GetProjectiles() };
        AttackType type = enemy->GetAttackType();

        std::vector<Projectile>& slots{ projectiles.Slots() };
        for (std::size_t i{ 0 }; i < slots.size(); ++i)
        {
            Projectile& bullet{ slots[i] };
            if (!bullet.isActive) continue;

            XMFLOAT3 currentPos = bullet.position;
            XMFLOAT3 currentVel = bullet.velocity;
            XMVECTOR vPos = XMLoadFloat3(&currentPos);
            XMVECTOR vVel = XMLoadFloat3(&currentVel);
            XMVECTOR vNextPos = vPos + (vVel * elapsedTime);
            XMVECTOR vDir = XMVector3Normalize(vVel);

            float speed = XMVectorGetX(XMVector3Length(vVel));
            float frameDist = speed * elapsedTime;
            float bulletRadius = bullet.radius;

            bool hitWall = false;
            float closestT = frameDist;
            XMVECTOR hitNormal = XMVectorZero();

            if (m_stage)
            {
                for (const auto& wall : m_stage->m_debugWalls)
                {
                    float dx = currentPos.x - wall.Position.x;
                    float dz = currentPos.z - wall.Position.z;
                    float wallMax = (std::max)(wall.Scale.x, wall.Scale.z);
                    if ((dx * dx + dz * dz) > pow(wallMax + frameDist + 10.0f, 2)) continue;

                    XMVECTOR tempNormal;
                    float t = RayCastOBB(vPos, vDir, frameDist, bulletRadius, wall, tempNormal);

                    if (t >= 0.0f && t < closestT)
                    {
                        closestT = t;
                        hitNormal = tempNormal;
                        hitWall = true;
                    }
                }
            }

            if (hitWall)
            {
                if (type == AttackType::Static) {
                    projectiles.Despawn(i);
                    continue;
                }

                float safeDist = (std::max)(0.0f, closestT - 0.01f);
                XMVECTOR vSafePos = vPos + (vDir * safeDist);
                XMVECTOR vReflectedVel = XMVector3Reflect(vVel, hitNormal);
                float remainingDist = frameDist - closestT;
                vSafePos += (XMVector3Normalize(vReflectedVel) * remainingDist);
                vSafePos += hitNormal * 0.05f;

                XMFLOAT3 finalPos, finalVel;
                XMStoreFloat3(&finalPos, vSafePos);
                XMStoreFloat3(&finalVel, vReflectedVel);
                finalPos.y = 0.0f; finalVel.y = 0.0f;

                bullet.position = finalPos;
                bullet.velocity = finalVel;
                continue;
            }

            bool hitPlayer = false;

            // ----------------------------------------------------
            // ENEMY BULLET VS PLAYER COLLISION
            // ----------------------------------------------------
            XMFLOAT3 nextPosFloat;
            XMStoreFloat3(&nextPosFloat, vNextPos);

            if (m_player && bullet.target == ProjectileTarget::none && m_player->GetHP() > 0)
            {
                DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();

                constexpr float PLAYER_HITBOX_RADIUS = 0.3f;

                float combinedRadius = PLAYER_HITBOX_RADIUS + bulletRadius;

                // Mathematical CCD (Prevents Tunneling)
                float distToPath = DistancePointToLineSegment2D(currentPos, nextPosFloat, playerPos);

                // Why TakeDamage is called while invincible: it detects the perfect dodge.
                // An ignored hit leaves the bullet flying through the player.
                if (distToPath <= combinedRadius &&
                    m_player->TakeDamage(DamageInfo{ m_damageConfig.enemyBulletToPlayer, currentPos }) != DamageResult::ignored)
                {
                    // Destroy the bullet
                    projectiles.Despawn(i);
                    continue;
                }
            }

            bullet.position = nextPosFloat;
        }
    }
}

float CollisionManager::GetEnemyPushRadius(const Enemy* enemy) const
{
    float scale = enemy->GetScale().x;

    switch (enemy->GetType())
    {
    case EnemyType::Pentagon:   return 4.0f * scale;
    case EnemyType::Paddle:     return 0.8f * scale;
    case EnemyType::FakeBoss:   return 1.9f * scale;
    default:                    return 1.2f * scale; 
    }
}

void CollisionManager::CheckPlayerVsEnemies()
{
    // FAST FAIL: Guard clauses
    if (!m_player || !m_enemyManager) return;

    // CPU OPTIMIZATION: If player is already dead, skip all enemy physics!
    if (m_player->GetHP() <= 0) return;

    auto& enemies{ m_enemyManager->GetEnemies() };
    DirectX::XMFLOAT3 playerPos{ m_player->GetMovement()->GetPosition() };
    DirectX::XMFLOAT3 playerVel{ m_player->GetMovement()->GetVelocity() };

    constexpr float PLAYER_RADIUS{ 0.25f };
    bool collidedAny{ false };

    for (const auto& enemy : enemies)
    {
        if (!enemy || !enemy->IsActive()) continue;

        const DirectX::XMFLOAT3 ePos{ enemy->GetPosition() };

        const float enemyRadius{ GetEnemyPushRadius(enemy.get()) };
        const float combinedRadius{ PLAYER_RADIUS + enemyRadius };

        // Zero-copy math
        float dx{ playerPos.x - ePos.x };
        float dz{ playerPos.z - ePos.z };
        const float distSq{ (dx * dx) + (dz * dz) };

        if (distSq < (combinedRadius * combinedRadius))
        {
            // =======================================================
            // THE KAMIKAZE INSTA-KILL MECHANIC 
            // =======================================================
            if (enemy->GetAttackType() == AttackType::Tracking && !m_player->IsInvincible())
            {
                // 1. Instantly nuke player HP
                m_player->TakeDamage(DamageInfo{ 9999, ePos });

                // 3. Kill the kamikaze enemy so it doesn't survive the explosion
                enemy->TakeDamage(DamageInfo{ 9999, ePos });

                enemy->SetKilledPlayer(true);

                // 4. INSTANT EXIT: Player is dead, absolutely zero need to check other enemies!
                return;
            }

            // =======================================================
            // NORMAL PUSH PHYSICS (For Static / Standard Enemies)
            // =======================================================
            float dist{ std::sqrt(distSq) };

            // Divide-By-Zero Guard (NaN propagation)
            if (dist < 0.0001f)
            {
                dx = 1.0f; dz = 0.0f; dist = 1.0f;
            }

            const float overlap{ combinedRadius - dist };
            playerPos.x += (dx / dist) * overlap;
            playerPos.z += (dz / dist) * overlap;

            collidedAny = true;

            // "Sticky Wall" Velocity Fix
            DirectX::XMVECTOR vVel{ DirectX::XMLoadFloat3(&playerVel) };
            const DirectX::XMVECTOR vNormal{ DirectX::XMVectorSet(dx / dist, 0.0f, dz / dist, 0.0f) };

            const float dot{ DirectX::XMVectorGetX(DirectX::XMVector3Dot(vVel, vNormal)) };
            if (dot < 0.0f)
            {
                vVel = DirectX::XMVectorSubtract(vVel, DirectX::XMVectorScale(vNormal, dot));
                DirectX::XMStoreFloat3(&playerVel, vVel);
            }
        }
    }

    // Only update memory if a collision actually happened
    if (collidedAny)
    {
        m_player->SetPosition(playerPos);
        m_player->GetMovement()->SetVelocity(playerVel);
    }
}

void CollisionManager::CheckPlayerVsItems()
{
    if (!m_player || !m_itemManager) return;

    XMFLOAT3 pPos = m_player->GetMovement()->GetPosition();
    float pRadius = 0.5f;

    for (auto& item : m_itemManager->GetItems())
    {
        if (!item->IsActive()) continue;

        XMFLOAT3 iPos = item->GetPosition();
        float iRadius = item->m_scale.x * 0.5f;

        float distSq = (pPos.x - iPos.x) * (pPos.x - iPos.x) +
            (pPos.z - iPos.z) * (pPos.z - iPos.z);

        float combinedRadius = pRadius + iRadius;

        if (distSq < combinedRadius * combinedRadius)
        {
            AudioManager::Instance().PlaySFX("Data/Sound/SE_Pop.wav", 0.4f);

            if (item->GetType() == ItemType::Invincible)
            {
            }

            item->SetActive(false);
        }
    }
}

void CollisionManager::CheckPlayerProjectilesVsEnemies(const float elapsedTime)
{
    // Only the player is required: its bullets must still reach the boss phase (the
    // Windowkill cage) when there are no enemies.
    if (!m_player) return;

    ProjectilePool& projectiles{ m_player->GetProjectiles() };

    constexpr float BULLET_HITBOX_RADIUS = 1.0f;

    std::vector<Projectile>& slots{ projectiles.Slots() };
    for (std::size_t i{ 0 }; i < slots.size(); ++i)
    {
        const Projectile& bullet{ slots[i] };
        if (!bullet.isActive) continue;

        const DirectX::XMFLOAT3 currentPos{ bullet.position };

        if (ICuePhase * phase{ m_Boss ? m_Boss->GetCurrentPhase() : nullptr }) {
            if (phase->TryAbsorbPlayerBullet(currentPos, bullet.damage)) {
                projectiles.Despawn(i);
                continue;
            }
        }

        // =========================================================
        // PLAYER BULLET VS ENEMIES
        // =========================================================
        if (!m_enemyManager) continue;
        const auto& enemies{ m_enemyManager->GetEnemies() };

        const DirectX::XMFLOAT3 velocity{ bullet.velocity };

        // Calculate where the bullet was last frame
        const DirectX::XMFLOAT3 prevPos{
            currentPos.x - (velocity.x * elapsedTime),
            currentPos.y - (velocity.y * elapsedTime),
            currentPos.z - (velocity.z * elapsedTime)
        };

        // 1. Generate the Swept AABB for the bullet
        const AABB bulletAABB{ CreateSweptAABB(prevPos, currentPos, BULLET_HITBOX_RADIUS) };

        for (const auto& enemy : enemies)
        {
            if (!enemy || !enemy->IsActive()) continue;

            const DirectX::XMFLOAT3 ePos{ enemy->GetPosition() };
            const float enemyRadius{ GetEnemyPushRadius(enemy.get()) };

            // 2. Generate the static AABB for the enemy
            const AABB enemyAABB{
                { ePos.x - enemyRadius, ePos.y - enemyRadius, ePos.z - enemyRadius },
                { ePos.x + enemyRadius, ePos.y + enemyRadius, ePos.z + enemyRadius }
            };

            // 3. BROAD-PHASE: Are they even close? 
            // This is a simple float comparison. It costs almost nothing.
            if (!CheckAABBIntersection(bulletAABB, enemyAABB))
            {
                continue; // Skip the expensive math entirely!
            }

            // 4. NARROW-PHASE: The expensive exact math (Only runs if broad-phase passes)
            if (CheckSphereCollision(currentPos, ePos, BULLET_HITBOX_RADIUS + enemyRadius))
            {
                enemy->TakeDamage(DamageInfo{ bullet.damage, currentPos });
                projectiles.Despawn(i);
                break; // Stop checking this bullet against other enemies
            }
        }
    }
}

void CollisionManager::CheckCueProjectilesVsEnemies(float elapsedTime)
{
    if (!m_cue || !m_enemyManager) return;

    ProjectilePool& projectiles{ m_cue->GetProjectiles() };
    auto& enemies = m_enemyManager->GetEnemies();

    constexpr float BULLET_HITBOX_RADIUS = 0.1f;


    const std::vector<Projectile>& slots{ projectiles.Slots() };
    for (std::size_t i{ 0 }; i < slots.size(); ++i)
    {
        if (!slots[i].isActive) continue;

        DirectX::XMFLOAT3 currentPos = slots[i].position;
        DirectX::XMFLOAT3 vel = slots[i].velocity;

        // Reconstruct last frame's position for the swept check.
        DirectX::XMFLOAT3 prevPos = {
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime), 
            currentPos.z - (vel.z * elapsedTime)
        };

        for (auto& enemy : enemies)
        {
            if (!enemy || !enemy->IsActive()) continue;

            DirectX::XMFLOAT3 ePos = enemy->GetPosition();

            // Scale the hitbox with the enemy's visual scale so it matches the model.
            float enemyScale = enemy->GetScale().x;
            float enemyRadius = 1.0f * enemyScale; // Default Ball

            if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
            else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 0.6f * enemyScale;

            // Combine the enemy's size with the bullet's size
            float exactHitDistance = enemyRadius + BULLET_HITBOX_RADIUS;

            // Swept check: hit if the enemy is within range of the segment from
            // prevPos to currentPos, so fast bullets cannot tunnel through.
            float distToPath = DistancePointToLineSegment2D(prevPos, currentPos, ePos);

            if (distToPath <= exactHitDistance)
            {
                enemy->TakeDamage(DamageInfo{ m_damageConfig.cueBulletToEnemy, currentPos });
                projectiles.Despawn(i);
                break; // Stop checking this bullet against other enemies
            }
        }
    }
}

void CollisionManager::CheckPlayerProjectilesVsCue(const float elapsedTime)
{
    // Fast fail: Early exit prevents unnecessary pointer dereferencing
    if (!m_player || !m_cue || !m_cue->IsAlive() || !m_cue->IsPotioned()) return;

    ProjectilePool& projectiles{ m_player->GetProjectiles() };
    const DirectX::XMFLOAT3 cuePos{ m_cue->GetMovement()->GetPosition() };

    constexpr float CUE_HITBOX_RADIUS_XZ{ 0.8f };

    const std::vector<Projectile>& slots{ projectiles.Slots() };
    for (std::size_t i{ 0 }; i < slots.size(); ++i)
    {
        if (!slots[i].isActive) continue;

        const DirectX::XMFLOAT3 currentPos{ slots[i].position };
        const DirectX::XMFLOAT3 vel{ slots[i].velocity };

        const DirectX::XMFLOAT3 prevPos{
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime),
            currentPos.z - (vel.z * elapsedTime)
        };

        // Anti-tunneling CCD (Continuous Collision Detection) 
        const float distToPath{ DistancePointToLineSegment2D(prevPos, currentPos, cuePos) };

        // 2D Cylinder Collision: Completely ignore the Y-axis vertical distance
        if (distToPath <= CUE_HITBOX_RADIUS_XZ)
        {
            m_cue->TakeDamage(DamageInfo{ m_damageConfig.playerBulletToCue, currentPos });
            EffectManager::Instance().Play("Data/Effect/Hit.efk", cuePos, 1.0f);
            projectiles.Despawn(i);
        }
    }
}

void CollisionManager::CheckCueAllyProjectilesVsPlayer(const float elapsedTime)
{
    // Skipped while the player is dead or Cue is not potioned. An invincible player is still
    // tested: TakeDamage detects the perfect dodge and the bullet flies on.
    if (!m_player || !m_cue || m_player->GetHP() <= 0 || !m_cue->IsPotioned()) return;

    // Reference bindings (No copying)
    ProjectilePool& projectiles{ m_cue->GetProjectiles() };
    const DirectX::XMFLOAT3 playerPos{ m_player->GetMovement()->GetPosition() };

    constexpr float PLAYER_HURTBOX_RADIUS{ 0.3f };

    const std::vector<Projectile>& slots{ projectiles.Slots() };
    for (std::size_t i{ 0 }; i < slots.size(); ++i)
    {
        if (!slots[i].isActive) continue;

        const DirectX::XMFLOAT3 currentPos{ slots[i].position };
        const DirectX::XMFLOAT3 vel{ slots[i].velocity };

        const DirectX::XMFLOAT3 prevPos{
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime),
            currentPos.z - (vel.z * elapsedTime)
        };

        const float distToPath{ DistancePointToLineSegment2D(prevPos, currentPos, playerPos) };
        const float combinedRadius{ PLAYER_HURTBOX_RADIUS + slots[i].radius };

        if (distToPath <= combinedRadius &&
            m_player->TakeDamage(DamageInfo{ m_damageConfig.cueBulletToPlayer, currentPos }) != DamageResult::ignored)
        {
            projectiles.Despawn(i); // Destroy the bullet
        }
    }
}

bool CollisionManager::CheckSphereCollision(const DirectX::XMFLOAT3& posA, const DirectX::XMFLOAT3& posB, float threshold)
{
    float dx = posA.x - posB.x;
    float dz = posA.z - posB.z;
    float distSq = (dx * dx) + (dz * dz);
    float thresholdSq = threshold * threshold;

    return distSq < thresholdSq;
}

void CollisionManager::CheckBossProjectilesVsPlayer(float elapsedTime)
{
    if (!m_player || !m_Boss || m_player->GetHP() <= 0) return;

    ICuePhase* phase{ m_Boss->GetCurrentPhase() };
    if (!phase) return;

    // Player hitbox radius against boss projectiles.
    constexpr float k_playerHitboxRadius{ 0.3f };

    // Damages the player when a projectile at `position` touches them.
    // Returns true when the projectile is used up.
    auto hitPlayer = [&](const DirectX::XMFLOAT3& position, float radius, int damage) {
        const DirectX::XMFLOAT3 pPos{ m_player->GetPosition() };

        // Full 3D distance (X, Y, Z), so attacks high above the player such as
        // Rain and Bijuudama do not hit the player below them.
        const float dx{ pPos.x - position.x };
        const float dy{ pPos.y - position.y };
        const float dz{ pPos.z - position.z };
        const float distSq{ (dx * dx) + (dy * dy) + (dz * dz) };
        const float totalRadius{ radius + k_playerHitboxRadius };
        if (distSq > (totalRadius * totalRadius)) return false;

        const DamageResult result{ m_player->TakeDamage(DamageInfo{ damage, position }) };
        // Why: an invincible player passes through. The projectile stays alive and there is
        // no hit feedback, the same as enemy and Cue bullets; a dash there is a perfect dodge.
        if (result == DamageResult::ignored) return false;

        CameraController::Instance().AddTrauma(0.2f);
        AudioManager::Instance().PlaySFX("Data/Sound/SE_Damage.wav", 0.3f);
        return true;
        };

    if (ProjectilePool * pool{ phase->GetProjectilePool() })
    {
        const std::vector<Projectile>& slots{ pool->Slots() };
        for (std::size_t i{ 0 }; i < slots.size(); ++i)
        {
            const Projectile& projectile{ slots[i] };
            if (!projectile.isActive) continue;
            // A projectile heading back to the boss never hurts the player.
            if (projectile.target == ProjectileTarget::boss) continue;
            if (!projectile.hitsPlayer) continue;

            if (hitPlayer(projectile.position, projectile.radius, projectile.damage))
            {
                pool->Despawn(i);
            }
        }
    }
}