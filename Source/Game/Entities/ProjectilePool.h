#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>

// Who a projectile steers toward. Target positions come from ProjectileTargets on each update.
enum class ProjectileTarget : std::uint8_t
{
    none,   // flies straight
    player, // homes on the player, leading by the player's velocity
    boss,   // homes on the boss; with isParabolic set, follows a curve that ends on the boss
    enemy,  // homes on the enemy named by targetId, leading by that enemy's velocity
};

// Position and velocity of one enemy that projectiles can home on.
struct TrackedTarget
{
    std::uint32_t id{ 0 };
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 velocity{};
};

// Target state the pool reads while steering. The owner fills it once per update.
struct ProjectileTargets
{
    DirectX::XMFLOAT3 playerPosition{};
    DirectX::XMFLOAT3 playerVelocity{};
    DirectX::XMFLOAT3 bossPosition{};
    // Active enemies. A projectile whose enemy is not listed flies straight.
    std::vector<TrackedTarget> enemies{};
};

// One pooled projectile. Plain hot data: the pool walks every slot each frame.
struct Projectile
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 velocity{};
    float yawDegrees{ 0.0f };   // facing of an attached effect while the projectile is not moving
    float radius{ 0.25f };      // hitbox radius, world units
    float visualScale{ 1.0f };  // uniform model scale
    int damage{ 10 };
    float age{ 0.0f };          // seconds since spawn
    float turnSpeed{ 8.0f };    // homing steering rate, per second
    ProjectileTarget target{ ProjectileTarget::none };
    std::uint32_t targetId{ 0 }; // enemy to home on when target == enemy; 0 names no enemy

    // Quadratic Bezier from parabolaStart through parabolaControl to the boss. Used with target == boss.
    bool isParabolic{ false };
    DirectX::XMFLOAT3 parabolaStart{};
    DirectX::XMFLOAT3 parabolaControl{};
    float parabolaTime{ 0.0f };
    float parabolaDuration{ 1.0f };

    int vfxHandle{ -1 };        // Effekseer handle, -1 when no effect is attached
    float vfxScaleMultiplier{ 1.0f };

    bool isActive{ false };
};

// Names one spawn of one slot. Becomes stale when that projectile is despawned,
// even if the slot is spawned again later.
struct ProjectileHandle
{
    static constexpr std::uint32_t k_invalidIndex{ 0xFFFFFFFFu };

    std::uint32_t index{ k_invalidIndex };
    std::uint32_t generation{ 0 };
};

// Fixed-capacity projectile pool in one contiguous buffer. Slots never move, so a
// handle stays valid until its projectile is despawned. Attached effects are stopped
// when a projectile is despawned and when the pool is destroyed.
class ProjectilePool
{
public:
    explicit ProjectilePool(std::size_t capacity);
    ~ProjectilePool();

    ProjectilePool(const ProjectilePool&) = delete;
    ProjectilePool& operator=(const ProjectilePool&) = delete;
    ProjectilePool(ProjectilePool&&) noexcept = default;
    ProjectilePool& operator=(ProjectilePool&&) noexcept = default;

    // Activates the free slot with the lowest index, reset to default values.
    // Returns an invalid handle when every slot is in use.
    ProjectileHandle Spawn();

    // Null when the handle is invalid or its projectile has been despawned.
    [[nodiscard]] Projectile* Get(ProjectileHandle handle);
    [[nodiscard]] const Projectile* Get(ProjectileHandle handle) const;

    // Handle for the projectile currently in slot `index`.
    [[nodiscard]] ProjectileHandle HandleOf(std::size_t index) const;

    void Despawn(std::size_t index);
    void Despawn(ProjectileHandle handle);
    void Clear();

    // Plays `path` at the projectile and keeps it on the projectile until the effect ends.
    void AttachVfx(std::size_t index, const char* path, float scale);
    void StopVfx(std::size_t index);

    // Moves every active projectile by `dt` seconds and keeps attached effects on it.
    void Update(float dt, const ProjectileTargets& targets);

    // Sets straight flight from `position` along `direction` (normalized here) at `speed`.
    static void Launch(Projectile& projectile, const DirectX::XMFLOAT3& position,
        const DirectX::XMFLOAT3& direction, float speed);

    // Every slot, active or not. Check `isActive` before using one.
    [[nodiscard]] std::vector<Projectile>& Slots() { return m_slots; }
    [[nodiscard]] const std::vector<Projectile>& Slots() const { return m_slots; }

private:
    std::vector<Projectile> m_slots;
    std::vector<std::uint32_t> m_generations; // one per slot, bumped on every despawn
};