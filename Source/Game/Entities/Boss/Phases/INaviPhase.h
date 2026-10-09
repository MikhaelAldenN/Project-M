#pragma once
#include <cstdint>
#include <d3d11.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include "DamageTypes.h"
#include "IBossAttackPattern.h"

class Boss;
class Bullet;
class Camera;
class Player;

// Which part of the boss fight a phase implements. Callers use it for decisions that
// depend on the fight's stage (main window hidden, post-process), not on one feature.
enum class BossPhaseKind : std::uint8_t
{
    bulletHell, // Act 2: fight inside the game image
    windowkill, // Act 3: OS windows on the desktop
};

class INaviPhase {
public:
    virtual ~INaviPhase() = default;

    virtual void Enter(Boss* boss) = 0;
    virtual void Update(float dt, Boss* boss) = 0;
    virtual void Render(ID3D11DeviceContext* context, Camera* currentCamera, Boss* boss) = 0;
    virtual void Exit(Boss* boss) = 0;

    [[nodiscard]] virtual BossPhaseKind GetKind() const = 0;

    // ----- Boss HP: owned by the active phase, reset in Enter -----
    [[nodiscard]] virtual int GetHP() const = 0;
    [[nodiscard]] virtual int GetMaxHP() const = 0;
    [[nodiscard]] virtual bool IsDead() const = 0;
    // Sets HP directly, without hit feedback (debug panel, respawn).
    virtual void SetHP(int hp) = 0;
    // Applies damage with hit feedback at `hitPos` (world space). Does nothing once dead.
    virtual DamageResult TakeDamage(const DamageInfo& damage) = 0;
    // False while player bullets must pass through without dealing damage.
    [[nodiscard]] virtual bool IsVulnerable() const { return !IsDead(); }

    // ----- AI -----
    // `target` is borrowed: the scene owns the player and outlives the phase.
    virtual void SetAITarget(Player* target) = 0;
    virtual void SetAIEnabled(bool isEnabled) = 0;
    [[nodiscard]] virtual bool IsAIEnabled() const = 0;

    // True once the phase's end sequence is over and the scene may leave the fight.
    [[nodiscard]] virtual bool IsReadyToChangeScene() const { return false; }

    // Appends the bullets that are active right now. `out` is not cleared, so one buffer
    // can be reused. Pointers are borrowed and valid until the phase's next Update.
    virtual void AppendActiveProjectiles(std::vector<Bullet*>& out) const = 0;

    // Takes ownership of `attack` and starts it. A null `attack` is ignored.
    virtual void AddAttack(std::unique_ptr<IBossAttackPattern> attack) = 0;
    // Optional hooks: the defaults fit a phase without attacks or AI target.
    virtual bool HasActiveAttacks() const { return false; }
    virtual Player* GetAITarget() const { return nullptr; }
};