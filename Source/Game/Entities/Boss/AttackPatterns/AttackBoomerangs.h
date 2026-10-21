#pragma once
#include "IBossAttackPattern.h"
#include "ProjectileMesh.h"
#include "ProjectilePool.h"
#include <memory>
#include <string>
#include <vector>

struct BoomerangParams {
    float speed = 40.0f;
    float windowSize = 150.0f;
    float visualScale = 5.0f;
    float hitboxRadius = 1.0f;
    int   spawnCount = 5;
    float spawnDelay = 1.0f;
    float turnSpeed = 3.0f;
    int   damage = 10;
    float maxTravelDistance = 35.0f;
    bool  spawnBottomHalfOnly = true;
};

class AttackBoomerangs : public IBossAttackPattern {
public:
    explicit AttackBoomerangs(const BoomerangParams& params);
    ~AttackBoomerangs() override = default;

    void Start(Boss* boss, ProjectilePool* pool) override;
    void Update(float dt, Boss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) override;
    void Stop(Boss* boss) override;

    bool IsFinished() const override;

private:
    struct BoomerangWindowBullet {
        ProjectileHandle projectile;
        std::string windowName;
        int state = 0;         // 0 = Entering, 1 = Returning
        int spawnSide = 1;     // 1 = Right, -1 = Left
        float startX = 0.0f;
        float targetX = 0.0f;
        float targetVelX = 0.0f;
    };

    BoomerangParams m_params;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    ProjectileMesh m_mesh;
    std::vector<BoomerangWindowBullet> m_bullets;

    bool  m_isSpawning = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;
};