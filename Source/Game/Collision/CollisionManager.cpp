#include "CollisionManager.h"
#include "Boss.h"
#include "CollisionGeometry.h"
#include "ProjectilePool.h"
#include "EffectManager.h"
#include "TimeManager.h"
#include <CameraController.h>
#include <limits>

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
    CheckPlayerProjectilesVsNavi(elapsedTime);
    CheckNaviAllyProjectilesVsPlayer(elapsedTime);
    CheckPlayerVsEnemies();

    if (m_itemManager)
    {
        CheckPlayerVsItems();
    }
    
    CheckNaviProjectilesVsEnemies(elapsedTime);
    CheckBossProjectilesVsPlayer(elapsedTime);
    CheckBossProjectilesVsBoss(elapsedTime);
    CheckPlayerProjectilesVsBoss();
}

void CollisionManager::CheckPlayerProjectilesVsBoss()
{
    if (!m_Boss || !m_player) return;

    INaviPhase* phase{ m_Boss->GetCurrentPhase() };
    if (!phase || !phase->IsVulnerable()) return;

    // The boss window is 5 x 5 world units, so half of that on each axis.
    constexpr float k_halfWidth{ 2.5f };
    constexpr float k_halfDepth{ 2.5f };
    const DirectX::XMFLOAT3 bossPos{ m_Boss->GetPosition() };

    for (auto& bullet : m_player->GetProjectiles()) {
        if (!bullet->IsActive()) continue;

        const DirectX::XMFLOAT3 bPos{ bullet->GetMovement()->GetPosition() };
        if (bPos.x > (bossPos.x - k_halfWidth) && bPos.x < (bossPos.x + k_halfWidth) &&
            bPos.z >(bossPos.z - k_halfDepth) && bPos.z < (bossPos.z + k_halfDepth))
        {
            bullet->SetActive(false);
            phase->TakeDamage(DamageInfo{ bullet->GetDamage(), bPos });
        }
    }
}

void CollisionManager::CheckEnemyProjectilesFull(float elapsedTime) 
{
    if (!m_enemyManager) return;

    for (auto& enemy : m_enemyManager->GetEnemies())
    {
        auto& projectiles = enemy->GetProjectiles();
        AttackType type = enemy->GetAttackType();

        for (auto it = projectiles.begin(); it != projectiles.end(); )
        {
            auto& bullet = *it;
            if (!bullet) { ++it; continue; }

            XMFLOAT3 currentPos = bullet->GetMovement()->GetPosition();
            XMFLOAT3 currentVel = bullet->GetVelocity();
            XMVECTOR vPos = XMLoadFloat3(&currentPos);
            XMVECTOR vVel = XMLoadFloat3(&currentVel);
            XMVECTOR vNextPos = vPos + (vVel * elapsedTime);
            XMVECTOR vDir = XMVector3Normalize(vVel);

            float speed = XMVectorGetX(XMVector3Length(vVel));
            float frameDist = speed * elapsedTime;
            float bulletRadius = bullet->GetRadius();

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
                    it = projectiles.erase(it);
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

                bullet->ApplyMovement(finalPos, finalVel);
                ++it;
                continue;
            }

            if (bullet->GetHomingTarget() != nullptr)
            {
                Enemy* targetEnemy = static_cast<Enemy*>(bullet->GetHomingTarget());

                // Verify the target wasn't deleted from memory by a different attack (like a Slash)
                bool isTargetAlive = false;
                for (auto& activeEnemy : m_enemyManager->GetEnemies()) {
                    if (activeEnemy.get() == targetEnemy && activeEnemy->IsActive()) {
                        isTargetAlive = true;
                        break;
                    }
                }

                // If the target died while the bullet was mid-air, destroy the bullet to prevent a crash.
                if (!isTargetAlive) {
                    it = projectiles.erase(it);
                    continue;
                }

                DirectX::XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();
                DirectX::XMFLOAT3 targetPos = targetEnemy->GetPosition();

                float dx = bPos.x - targetPos.x;
                float dz = bPos.z - targetPos.z;
                float distSq = dx * dx + dz * dz;

                // Hit radius scales with the enemy's visual scale.
                float enemyScale = targetEnemy->GetScale().x;
                float hitRadius = 1.0f * enemyScale;

                if (targetEnemy->GetType() == EnemyType::Pentagon) hitRadius = 4.0f * enemyScale;
                else if (targetEnemy->GetType() == EnemyType::Paddle) hitRadius = 0.8f * enemyScale;

                float combinedHitRadius = hitRadius + bullet->GetRadius();

                if (distSq < (combinedHitRadius * combinedHitRadius))
                {
                    // The parried bullet damages the enemy.
                    targetEnemy->TakeDamage(DamageInfo{ m_damageConfig.parriedBulletToEnemy, bPos });

                    // Destroy the bullet
                    it = projectiles.erase(it);
                    continue;
                }
            }

            bool hitPlayer = false;

            // ----------------------------------------------------
            // ENEMY BULLET VS PLAYER COLLISION
            // ----------------------------------------------------
            XMFLOAT3 nextPosFloat;
            XMStoreFloat3(&nextPosFloat, vNextPos);

            if (m_player && bullet->GetHomingTarget() == nullptr && m_player->GetHP() > 0 && !m_player->IsInvincible())
            {
                DirectX::XMFLOAT3 playerPos = m_player->GetMovement()->GetPosition();

                constexpr float PLAYER_HITBOX_RADIUS = 0.3f;

                float combinedRadius = PLAYER_HITBOX_RADIUS + bulletRadius;

                // Mathematical CCD (Prevents Tunneling)
                float distToPath = DistancePointToLineSegment2D(currentPos, nextPosFloat, playerPos);

                if (distToPath <= combinedRadius)
                {
                    m_player->TakeDamage(DamageInfo{ m_damageConfig.enemyBulletToPlayer, currentPos });

                    // Destroy the bullet and prevent crashes
                    it = projectiles.erase(it);
                    continue;
                }
            }

            bullet->ApplyMovement(nextPosFloat, currentVel);
            ++it;
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
        float iRadius = item->scale.x * 0.5f;

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
    // Cukup cek player di awal, agar logic menembak kandang tetap jalan 
    // meskipun tidak ada musuh (atau m_enemyManager belum siap)
    if (!m_player) return;

    auto& projectiles{ m_player->GetProjectiles() };

    constexpr float BULLET_HITBOX_RADIUS = 1.0f;

    for (auto& bullet : projectiles)
    {
        if (!bullet || !bullet->IsActive()) continue;

        const DirectX::XMFLOAT3 currentPos{ bullet->GetMovement()->GetPosition() };

        if (INaviPhase * phase{ m_Boss ? m_Boss->GetCurrentPhase() : nullptr }) {
            if (phase->TryAbsorbPlayerBullet(currentPos, bullet->GetDamage())) {
                bullet->SetActive(false);
                continue;
            }
        }

        // =========================================================
        // DETEKSI PELURU VS MUSUH (Logika Aslimu)
        // =========================================================
        if (!m_enemyManager) continue; // Cek musuh di sini agar aman
        const auto& enemies{ m_enemyManager->GetEnemies() };

        const DirectX::XMFLOAT3 velocity{ bullet->GetVelocity() };

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
                enemy->TakeDamage(DamageInfo{ bullet->GetDamage(), currentPos });
                bullet->SetActive(false);
                break; // Stop checking this bullet against other enemies
            }
        }
    }
}

void CollisionManager::CheckNaviProjectilesVsEnemies(float elapsedTime)
{
    if (!m_navi || !m_enemyManager) return;

    auto& projectiles = m_navi->GetProjectiles();
    auto& enemies = m_enemyManager->GetEnemies();

    constexpr float BULLET_HITBOX_RADIUS = 0.1f;


    for (auto& bullet : projectiles)
    {
        if (!bullet || !bullet->IsActive()) continue;

        DirectX::XMFLOAT3 currentPos = bullet->GetMovement()->GetPosition();
        DirectX::XMFLOAT3 vel = bullet->GetVelocity();

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
                enemy->TakeDamage(DamageInfo{ m_damageConfig.naviBulletToEnemy, currentPos });
                bullet->SetActive(false); // Send back to Object Pool instantly
                break; // Stop checking this bullet against other enemies
            }
        }
    }
}

void CollisionManager::CheckPlayerProjectilesVsNavi(const float elapsedTime)
{
    // Fast fail: Early exit prevents unnecessary pointer dereferencing
    if (!m_player || !m_navi || !m_navi->IsAlive() || !m_navi->IsPotioned()) return;

    // Reference binding to avoid copying the container
    const auto& projectiles{ m_player->GetProjectiles() };
    const DirectX::XMFLOAT3 naviPos{ m_navi->GetMovement()->GetPosition() };

    constexpr float NAVI_HITBOX_RADIUS_XZ{ 0.8f };

    // CPU Optimization: Range-based for loop
    for (const auto& bullet : projectiles)
    {
        // Null and active state guard
        if (!bullet || !bullet->IsActive()) continue;

        const DirectX::XMFLOAT3 currentPos{ bullet->GetMovement()->GetPosition() };
        const DirectX::XMFLOAT3 vel{ bullet->GetVelocity() };

        const DirectX::XMFLOAT3 prevPos{
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime),
            currentPos.z - (vel.z * elapsedTime)
        };

        // Anti-tunneling CCD (Continuous Collision Detection) 
        const float distToPath{ DistancePointToLineSegment2D(prevPos, currentPos, naviPos) };

        // 2D Cylinder Collision: Completely ignore the Y-axis vertical distance
        if (distToPath <= NAVI_HITBOX_RADIUS_XZ)
        {
            m_navi->TakeDamage(DamageInfo{ m_damageConfig.playerBulletToNavi, currentPos });
            EffectManager::Instance().Play("Data/Effect/Hit.efk", naviPos, 1.0f);
            bullet->SetActive(false); // Instantly recycle the bullet into the object pool
        }
    }
}

void CollisionManager::CheckNaviAllyProjectilesVsPlayer(const float elapsedTime)
{
    // If the player is dead, Navi isn't potioned, OR the player is currently Dashing (Invincible),
    if (!m_player || !m_navi || m_player->GetHP() <= 0 || !m_navi->IsPotioned() || m_player->IsInvincible()) return;

    // Reference bindings (No copying)
    const auto& projectiles{ m_navi->GetProjectiles() };
    const DirectX::XMFLOAT3 playerPos{ m_player->GetMovement()->GetPosition() };

    constexpr float PLAYER_HURTBOX_RADIUS{ 0.3f };

    for (const auto& bullet : projectiles)
    {
        if (!bullet || !bullet->IsActive()) continue;

        const DirectX::XMFLOAT3 currentPos{ bullet->GetMovement()->GetPosition() };
        const DirectX::XMFLOAT3 vel{ bullet->GetVelocity() };

        const DirectX::XMFLOAT3 prevPos{
            currentPos.x - (vel.x * elapsedTime),
            currentPos.y - (vel.y * elapsedTime),
            currentPos.z - (vel.z * elapsedTime)
        };

        const float distToPath{ DistancePointToLineSegment2D(prevPos, currentPos, playerPos) };
        const float combinedRadius{ PLAYER_HURTBOX_RADIUS + bullet->GetRadius() };

        if (distToPath <= combinedRadius)
        {
            bullet->SetActive(false); // Destroy the bullet

            m_player->TakeDamage(DamageInfo{ m_damageConfig.naviBulletToPlayer, currentPos });
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

Enemy* CollisionManager::GetTargetInSlashCone(const DirectX::XMFLOAT3& playerPos, const DirectX::XMFLOAT3& aimDir, float baseReach, float minDotProduct) const
{
    if (!m_enemyManager) return nullptr;

    Enemy* bestTarget{ nullptr };
    float closestDistSq{ (std::numeric_limits<float>::max)() };

    for (const auto& enemy : m_enemyManager->GetEnemies())
    {
        if (!enemy || !enemy->IsActive()) continue;

        const DirectX::XMFLOAT3 ePos{ enemy->GetPosition() };
        const float enemyScale{ enemy->GetScale().x };
        float enemyRadius{ 1.0f * enemyScale };

        if (enemy->GetType() == EnemyType::Pentagon) enemyRadius = 4.0f * enemyScale;
        else if (enemy->GetType() == EnemyType::Paddle) enemyRadius = 1.2f * enemyScale;

        // =========================================================
        // DYNAMIC HITBOX & TERNARY OPTIMIZATION
        // Kamikazes get a 3.5x reach multiplier to combat tunneling.
        // Using const initialization ensures zero mutation overhead.
        // =========================================================
        const bool isKamikaze{ enemy->GetAttackType() == AttackType::Tracking };
        const float dynamicReach{ isKamikaze ? (baseReach * 3.5f) : baseReach };

        const float exactSlashDistance{ 0.5f + enemyRadius + dynamicReach };
        const float exactSlashDistSq{ exactSlashDistance * exactSlashDistance };

        const float dx{ ePos.x - playerPos.x };
        const float dz{ ePos.z - playerPos.z };
        const float distSq{ (dx * dx) + (dz * dz) };

        // Fast fail distance check before expensive sqrt
        if (distSq < exactSlashDistSq && distSq > 0.0001f)
        {
            const float dist{ std::sqrt(distSq) };
            const float dirX{ dx / dist };
            const float dirZ{ dz / dist };

            const float dot{ (dirX * aimDir.x) + (dirZ * aimDir.z) };

            // STRICT FACING CHECK: If the dot product is less than the threshold 
            // (e.g. Kamikaze is behind the player), this fails entirely. Player dies.
            if (dot >= minDotProduct)
            {
                // =========================================================
                // TARGET PRIORITIZATION (SHIELDING BUG PREVENTION)
                // If it's a Kamikaze, we artificially multiply its distance
                // by 0.1f during the comparison. This guarantees the Kamikaze
                // wins the `bestTarget` check over standard enemies.
                // =========================================================
                const float prioritizationDistSq{ isKamikaze ? (distSq * 0.1f) : distSq };

                if (prioritizationDistSq < closestDistSq)
                {
                    closestDistSq = prioritizationDistSq;
                    bestTarget = enemy.get();
                }
            }
        }
    }

    return bestTarget;
}

bool CollisionManager::GetParryableProjectile(const XMFLOAT3& playerPos, float threshold, Bullet** outBullet, Enemy** outNearestEnemy, XMFLOAT3* outPosition)
{
    // 1. Cek Musuh Biasa (JIKA ADA)
    if (m_enemyManager)
    {
        for (auto& enemy : m_enemyManager->GetEnemies())
        {
            if (enemy->GetAttackType() != AttackType::Tracking) continue;

            for (auto& bullet : enemy->GetProjectiles())
            {
                if (!bullet->IsActive()) continue;

                XMFLOAT3 bPos = bullet->GetMovement()->GetPosition();

                if (CheckSphereCollision(playerPos, bPos, threshold))
                {
                    if (outBullet) *outBullet = bullet.get();
                    if (outPosition) *outPosition = bPos;

                    Enemy* nearest = nullptr;
                    float closestDistSq = 999999.0f;
                    for (auto& potential : m_enemyManager->GetEnemies())
                    {
                        if (!potential->IsActive()) continue;
                        XMFLOAT3 targetPos = potential->GetPosition();
                        float distSq = pow(playerPos.x - targetPos.x, 2) + pow(playerPos.z - targetPos.z, 2);
                        if (distSq < closestDistSq) {
                            closestDistSq = distSq;
                            nearest = potential.get();
                        }
                    }
                    if (outNearestEnemy) *outNearestEnemy = nearest ? nearest : enemy.get();

                    return true;
                }
            }
        }
    }

    // =========================================================
    // 2. DETEKSI BIJUUDAMA NAVI BOSS
    // =========================================================
    if (INaviPhase * phase{ m_Boss ? m_Boss->GetCurrentPhase() : nullptr })
    {
        // The attack decides whether it can be parried now, and shatters itself if so.
        if (const std::optional<XMFLOAT3> ballPosition{ phase->TryParryAttack(playerPos, m_Boss) })
        {
            if (outBullet) *outBullet = nullptr;
            if (outNearestEnemy) *outNearestEnemy = nullptr;
            if (outPosition) *outPosition = *ballPosition;
            return true;
        }
    }

    return false;
}

void CollisionManager::CheckBossProjectilesVsPlayer(float elapsedTime)
{
    if (!m_player || !m_Boss || m_player->GetHP() <= 0) return;

    INaviPhase* phase{ m_Boss->GetCurrentPhase() };
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
        // Why: an invincible player (dash) passes through. The projectile stays alive and
        // there is no hit feedback, the same as enemy and Navi bullets.
        if (result == DamageResult::ignored) return false;

        CameraController::Instance().AddTrauma(0.2f);
        AudioManager::Instance().PlaySFX("Data/Sound/SE_Damage.wav", 0.3f);
        return true;
        };

    m_bossBulletScratch.clear();
    phase->AppendActiveProjectiles(m_bossBulletScratch);
    for (Bullet* bullet : m_bossBulletScratch)
    {
        if (!bullet || !bullet->IsActive()) continue;
        // A parried bullet (heading back to the boss) never hurts the player.
        if (bullet->GetBossTarget() != nullptr) continue;

        if (hitPlayer(bullet->GetMovement()->GetPosition(), bullet->GetRadius(), bullet->GetDamage()))
        {
            bullet->SetActive(false);
        }
    }

    if (ProjectilePool * pool{ phase->GetProjectilePool() })
    {
        const std::vector<Projectile>& slots{ pool->Slots() };
        for (std::size_t i{ 0 }; i < slots.size(); ++i)
        {
            const Projectile& projectile{ slots[i] };
            if (!projectile.isActive) continue;
            // A projectile heading back to the boss never hurts the player.
            if (projectile.target == ProjectileTarget::boss) continue;

            if (hitPlayer(projectile.position, projectile.radius, projectile.damage))
            {
                pool->Despawn(i);
            }
        }
    }
}

void CollisionManager::CheckBossProjectilesVsBoss(float elapsedTime)
{
    if (!m_Boss) return;
    INaviPhase* phase{ m_Boss->GetCurrentPhase() };
    // Why bullet hell only: the parry that sends bullets back (GetParryableProjectile) exists only there.
    if (!phase || phase->GetKind() != BossPhaseKind::bulletHell) return;

    // Boss hitbox radius for projectiles sent back by a parry.
    constexpr float k_bossHitboxRadius{ 4.0f };

    // Full 3D check: include Y so parabolic shards passing right above the boss's head
    // do not explode in mid-air.
    auto reachesBoss = [&](const DirectX::XMFLOAT3& position) {
        const DirectX::XMFLOAT3 bossPos{ m_Boss->GetPosition() };
        const float dx{ position.x - bossPos.x };
        const float dy{ position.y - bossPos.y };
        const float dz{ position.z - bossPos.z };
        return ((dx * dx) + (dy * dy) + (dz * dz)) <= (k_bossHitboxRadius * k_bossHitboxRadius);
        };

    // Routes damage through the phase so the HP bar, sound, and damage flash stay in sync,
    // with a small shake for every fragment that lands.
    auto damageBoss = [&](const DirectX::XMFLOAT3& position, int damage) {
        phase->TakeDamage(DamageInfo{ damage, position });
        CameraController::Instance().AddTrauma(0.15f);
        };

    m_bossBulletScratch.clear();
    phase->AppendActiveProjectiles(m_bossBulletScratch);
    for (Bullet* bullet : m_bossBulletScratch)
    {
        if (!bullet->IsActive() || bullet->GetBossTarget() != m_Boss) continue;

        const DirectX::XMFLOAT3 bPos{ bullet->GetMovement()->GetPosition() };
        if (!reachesBoss(bPos)) continue;

        // Deactivated before the damage, so one fragment cannot hit twice.
        bullet->SetActive(false);
        damageBoss(bPos, bullet->GetDamage());
    }

    if (ProjectilePool * pool{ phase->GetProjectilePool() })
    {
        const std::vector<Projectile>& slots{ pool->Slots() };
        for (std::size_t i{ 0 }; i < slots.size(); ++i)
        {
            const Projectile& projectile{ slots[i] };
            if (!projectile.isActive || projectile.target != ProjectileTarget::boss) continue;
            if (!reachesBoss(projectile.position)) continue;

            const DirectX::XMFLOAT3 position{ projectile.position };
            const int damage{ projectile.damage };
            // Despawned before the damage, so one fragment cannot hit twice.
            pool->Despawn(i);
            damageBoss(position, damage);
        }
    }
}