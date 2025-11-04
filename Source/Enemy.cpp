#include "Enemy.h"
#include "EnemyManager.h"

// =============================================================
// Enemy: Remove itself from manager
void Enemy::Destroy()
{
	EnemyManager::Instance().Remove(this);
}