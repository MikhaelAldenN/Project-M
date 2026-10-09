#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

class Boss;
class Bullet;
class Camera;
struct ID3D11DeviceContext;

// Bullets shared by all attacks of one phase. Owned by the phase.
using BossBulletPool = std::vector<std::unique_ptr<Bullet>>;

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
    // shared bullet pool; it is null in Windowkill, whose attacks own their bullets.
    virtual void Start(Boss* boss, BossBulletPool* pool) = 0;

    // Processes bullet physics, window tracking, and spawning logic.
    virtual void Update(float dt, Boss* boss) = 0;

    // Renders specific 3D models or UI related to this attack.
    virtual void Render(ID3D11DeviceContext* context, Camera* camera, Boss* boss) = 0;

    // Cleans up tracking windows and VFX if the attack is forcefully stopped.
    virtual void Stop(Boss* boss) = 0;

    // Returns true when all projectiles are destroyed and the attack is over.
    virtual bool IsFinished() const = 0;

    // Returns active bullets for the global collision manager.
    virtual std::vector<Bullet*> GetActiveProjectiles() const = 0;

    // Non-null while the attack steers the boss; the phase then stops its idle hover.
// The pointer stays valid until the attack is destroyed.
    [[nodiscard]] virtual const BossMoveTarget* GetBossMoveTarget() const { return nullptr; }
};