#pragma once
#include "IBossAttackPattern.h"
#include "ProjectileMesh.h"
#include "ProjectilePool.h"
#include <memory>
#include <string>
#include <vector>
#include "Player.h"

struct UndyneSpearParams {
    int   count = 10;
    float spawnDelay = 0.4f;
    float hoverDuration = 1.0f;
    float startSpeed = 10.0f;
    float maxSpeed = 60.0f;
    float acceleration = 10.0f;
    int   damage = 10;
    float ringRadius = 8.0f;   // USULAN DESAIN: distance from the player at which a spear appears
    float angleStep = 137.5f;  // degrees between consecutive spears; 137.5 (golden angle) never clumps
};

class AttackSpears : public IBossAttackPattern {
public:
    AttackSpears(const UndyneSpearParams& params, Player* target);
    ~AttackSpears() override = default;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

private:
    struct UndyneSpearWindow {
        ProjectileHandle projectile;
        std::string windowName;
        int state = 0; // 0 = Hover/Aim, 2 = Launch
        float timer = 0.0f;
        DirectX::XMFLOAT3 lockDir = { 0,0,1 };
        float currentSpeed = 0.0f;
        bool isPreparedForDestroy = false;
    };

    // Despawns the spear's projectile and destroys its window. The spear then counts as finished.
    void RetireSpear(UndyneSpearWindow& spear, CueBoss* boss);

    UndyneSpearParams m_params;
    Player* m_playerTarget;
    ProjectilePool* m_pool = nullptr; // borrowed from the phase, set in Start
    ProjectileMesh m_mesh;
    std::vector<UndyneSpearWindow> m_spears;

    bool  m_isSpawning = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;
};