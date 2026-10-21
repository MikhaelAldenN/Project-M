#pragma once

#include <algorithm>
#include <cmath>
#include <DirectXMath.h>
#include <functional>
#include <vector>
#include "System/AudioManager.h"
#include "System/Input.h"
#include "Enemy.h"
#include "Player.h"
#include "Stage.h"
#include "EnemyManager.h"
#include "ItemManager.h"
#include "StateMachine.h"
#include "PlayerStates.h"
#include "NaviAlly.h"

class ItemManager;
class NaviAlly;
class Boss;

// Damage dealt by collision pairs whose bullets do not carry their own damage value.
// Boss projectiles and player bullets against enemies or the boss use Projectile::damage instead.
struct CollisionDamageConfig
{
    int enemyBulletToPlayer{ 10 };
    int parriedBulletToEnemy{ 30 };
    int naviBulletToEnemy{ 1 };
    int naviBulletToPlayer{ 10 };
    int playerBulletToNavi{ 10 };
};

class CollisionManager
{
public:
    CollisionManager() = default;
    ~CollisionManager() = default;

    // The boss is optional and set separately with SetBoss().
    void Initialize(Player* p, Stage* s, EnemyManager* em, ItemManager* im);

    void Update(float elapsedTime);
    [[nodiscard]] float GetEnemyPushRadius(const Enemy* enemy) const;
    [[nodiscard]] Enemy* GetTargetInSlashCone(const DirectX::XMFLOAT3& playerPos, const DirectX::XMFLOAT3& aimDir, float reach, float minDotProduct) const;
    // Finds what a parry at `playerPos` hits. `outPosition` receives where the parried projectile was.
    // `outEnemyBullet` and `outNearestEnemy` stay null for a boss attack, which handles its own parry.
    // `outEnemyBullet` points into the enemy's pool: use it at once, before that slot can be despawned or reused.
    bool GetParryableProjectile(const DirectX::XMFLOAT3& playerPos, float threshold, Projectile** outEnemyBullet, Enemy** outNearestEnemy, DirectX::XMFLOAT3* outPosition);
    void SetNavi(NaviAlly* navi) { m_navi = navi; }
    NaviAlly* GetNavi() const { return m_navi; }
    void SetBoss(Boss* Boss) { m_Boss = Boss; }
    Boss* GetBoss() const { return m_Boss; }

    // Mutable on purpose: a debug panel can tune the values at runtime.
    [[nodiscard]] CollisionDamageConfig& GetDamageConfig() { return m_damageConfig; }

private:
    void CheckPlayerVsEnemies();
    void CheckPlayerVsItems();
    void CheckPlayerProjectilesVsEnemies(float elapsedTime);
    void CheckPlayerProjectilesVsNavi(float elapsedTime);
    bool CheckSphereCollision(const DirectX::XMFLOAT3& posA, const DirectX::XMFLOAT3& posB, float threshold);
    void CheckEnemyProjectilesFull(float elapsedTime);
    void CheckNaviProjectilesVsEnemies(float elapsedTime);
    void CheckNaviAllyProjectilesVsPlayer(float elapsedTime);
    void CheckBossProjectilesVsPlayer(float elapsedTime);
    void CheckBossProjectilesVsBoss(float elapsedTime); // parried projectiles sent back at the boss
    // Player bullets against the boss window (point inside an XZ box).
    void CheckPlayerProjectilesVsBoss();

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;

    EnemyManager* m_enemyManager = nullptr;
    ItemManager* m_itemManager = nullptr;
    NaviAlly* m_navi = nullptr;
    Boss* m_Boss = nullptr;

    CollisionDamageConfig m_damageConfig{};
};