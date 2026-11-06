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
#include "CueAlly.h"

class ItemManager;
class CueAlly;
class CueBoss;

// Damage dealt by collision pairs whose bullets do not carry their own damage value.
// Boss projectiles and player bullets against enemies or the boss use Projectile::damage instead.
struct CollisionDamageConfig
{
    int enemyBulletToPlayer{ 10 };
    int cueBulletToEnemy{ 1 };
    int cueBulletToPlayer{ 10 };
    int playerBulletToCue{ 10 };
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
    void SetCue(CueAlly* cue) { m_cue = cue; }
    CueAlly* GetCue() const { return m_cue; }
    void SetBoss(CueBoss* boss) { m_Boss = boss; }
    CueBoss* GetBoss() const { return m_Boss; }

    // Mutable on purpose: a debug panel can tune the values at runtime.
    [[nodiscard]] CollisionDamageConfig& GetDamageConfig() { return m_damageConfig; }

private:
    void CheckPlayerVsEnemies();
    void CheckPlayerVsItems();
    void CheckPlayerProjectilesVsEnemies(float elapsedTime);
    void CheckPlayerProjectilesVsCue(float elapsedTime);
    bool CheckSphereCollision(const DirectX::XMFLOAT3& posA, const DirectX::XMFLOAT3& posB, float threshold);
    void CheckEnemyProjectilesFull(float elapsedTime);
    void CheckCueProjectilesVsEnemies(float elapsedTime);
    void CheckCueAllyProjectilesVsPlayer(float elapsedTime);
    void CheckBossProjectilesVsPlayer(float elapsedTime);
    // Player bullets against the boss window (point inside an XZ box).
    void CheckPlayerProjectilesVsBoss();

    Player* m_player = nullptr;
    Stage* m_stage = nullptr;

    EnemyManager* m_enemyManager = nullptr;
    ItemManager* m_itemManager = nullptr;
    CueAlly* m_cue = nullptr;
    CueBoss* m_Boss = nullptr;

    CollisionDamageConfig m_damageConfig{};
};