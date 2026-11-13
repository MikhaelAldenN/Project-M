#pragma once
#include "IBossAttackPattern.h"
#include "Primitive.h"
#include "System/Model.h"
#include <memory>
#include <string>

class Player;

struct BlasterParams {
    float cannonWindowSize = 200.0f;
    float cannonVisualScale = 2.0f;
    float beamVisualWidth = 2.0f;
    float beamMaxLength = 120.0f;
    float beamGrowSpeed = 20.0f;
    float beamSlideSpeed = 10.0f;
    int   beamDamage = 20;

    float chargeDelay = 1.0f;
    float fireDuration = 0.8f;
    int   spawnCount = 4;
    float spawnDelay = 0.5f;
    float spawnSpreadX = 40.0f;
    float dropInDuration = 0.4f;
    float retreatSpeed = 40.0f;
    float windowFadeSpeed = 25.0f;

    std::string chargeEffectPath = "Data/Effect/TEST.efk";
    std::string fireEffectPath = "Data/Effect/LASER.efk";
    float chargeEffectScale = 1.0f;
    float fireEffectScale = 1.0f;
    float effectPitchDegrees = 90.0f;
    DirectX::XMFLOAT3 effectOffset = { 0.0f, 0.0f, 2.0f };
};

class AttackBlasters : public IBossAttackPattern {
public:
    // With a target, each cannon drops at the target's x at the moment it spawns.
    // Without one, the cannons spread evenly over spawnSpreadX around x = 0.
    // `target` is borrowed: the scene owns the player and outlives the attack.
    // A firing beam damages the target; with no target the beams are harmless.
    AttackBlasters(const BlasterParams& params, Player* target);
    ~AttackBlasters() override = default;

    void Start(CueBoss* boss, ProjectilePool* pool) override;
    void Update(float dt, CueBoss* boss) override;
    void Render(ID3D11DeviceContext* context, Camera* camera, CueBoss* boss) override;
    void Stop(CueBoss* boss) override;

    bool IsFinished() const override;

private:
    struct OrbitalBlaster {
        bool active = true;
        int state = 1; // 1:DropIn, 2:Charge, 3:Fire, 4:Retreat
        float timer = 0.0f;
        float baseX = 0.0f;
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 targetPos = { 0.0f, 0.0f, 0.0f };
        float beamScaleX = 0.0f;
        float beamCurrentLength = 0.0f;

        std::string windowName;
        std::string beamWindowName;

        int chargeEffectHandle = -1;
        int fireEffectHandle = -1;
    };

    // Hits the target while it overlaps the blaster's firing beam (visible width plus
    // the player's collision radius).
    void DamageTargetInBeam(const OrbitalBlaster& blaster);

    BlasterParams m_params;
    Player* m_target{ nullptr };

    std::vector<std::shared_ptr<OrbitalBlaster>> m_blasters;
    bool  m_isSpawning = false;
    int   m_spawnedCount = 0;
    float m_spawnTimer = 0.0f;

    std::unique_ptr<Primitive> m_solidRenderer;
    std::shared_ptr<Model> m_placeholderModel;
};