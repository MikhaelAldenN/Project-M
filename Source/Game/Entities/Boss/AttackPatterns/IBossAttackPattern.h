#pragma once
#include <DirectXMath.h>
#include <memory>
#include <optional>
#include <vector>

class Boss;
class Camera;
struct ID3D11DeviceContext;

class ProjectilePool;

// Projectiles shared by all attacks of one phase. Owned by the phase.
using BossBulletPool = ProjectilePool;

struct BossMoveTarget {
    DirectX::XMFLOAT3 position{};
    float             lerpSpeed{ 0.0f };
};

// Base interface for all modular boss attacks to enforce SRP and DRY principles.
class IBossAttackPattern {
public:
    virtual ~IBossAttackPattern() = default;

    // Starts the attack: resources, tracking windows, VFX, first bullets.
    // `boss` and `pool` are borrowed and outlive the attack. `pool` is the running phase's
    // shared projectile pool.
    virtual void Start(Boss* boss, BossBulletPool* pool) = 0;

    // Processes bullet physics, window tracking, and spawning logic.
    virtual void Update(float dt, Boss* boss) = 0;

    // Renders specific 3D models or UI related to this attack.
    virtual void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) = 0;

    // Cleans up tracking windows and VFX if the attack is forcefully stopped.
    virtual void Stop(Boss* boss) = 0;

    // Returns true when all projectiles are destroyed and the attack is over.
    virtual bool IsFinished() const = 0;

    // Non-null while the attack steers the boss; the phase then stops its idle hover.
    // The pointer stays valid until the attack is destroyed.
    [[nodiscard]] virtual const BossMoveTarget* GetBossMoveTarget() const { return nullptr; }

    // The player parried at `parryPosition`. Returns where the parried projectile was,
    // or nothing when this attack has nothing to parry right now.
    virtual std::optional<DirectX::XMFLOAT3> TryParry(const DirectX::XMFLOAT3& /*parryPosition*/, Boss* /*boss*/) { return std::nullopt; }
};