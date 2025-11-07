#include "Enemy.h"
#include "EnemyManager.h"

void Enemy::Destroy()
{
	EnemyManager::Instance().Remove(this);
}