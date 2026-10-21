#include "ProjectilePool.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <optional>
#include "EffectManager.h"

using namespace DirectX;

namespace
{
    // Inside this distance homing turns harder, so a projectile cannot orbit its target.
    constexpr float k_closeSteerDistance{ 5.0f };
    constexpr float k_minSteerDistance{ 0.1f };
    // Below this speed an attached effect faces along yawDegrees instead of the velocity.
    constexpr float k_minFacingSpeed{ 0.1f };
    constexpr float k_minParabolaStep{ 0.0001f };

    // Moves `p` along its Bezier curve and derives `velocity` from the step,
    // so code that reads the velocity still sees the motion.
    void AdvanceAlongParabola(Projectile& p, float dt, const XMFLOAT3& end)
    {
        p.parabolaTime += dt;
        const float t{ (std::min)(1.0f, p.parabolaTime / p.parabolaDuration) };
        const float u{ 1.0f - t };
        const float uu{ u * u };
        const float ut2{ 2.0f * u * t };
        const float tt{ t * t };

        const XMFLOAT3 next{
            (uu * p.parabolaStart.x) + (ut2 * p.parabolaControl.x) + (tt * end.x),
            (uu * p.parabolaStart.y) + (ut2 * p.parabolaControl.y) + (tt * end.y),
            (uu * p.parabolaStart.z) + (ut2 * p.parabolaControl.z) + (tt * end.z),
        };

        const float step{ (std::max)(dt, k_minParabolaStep) };
        p.velocity = {
            (next.x - p.position.x) / step,
            (next.y - p.position.y) / step,
            (next.z - p.position.z) / step,
        };
        p.position = next;
    }

    struct TargetMotion
    {
        XMFLOAT3 position{};
        XMFLOAT3 velocity{};
    };

    // Where the target of `p` is and how it moves. Empty when `p` has no target
    // or its enemy is not in `targets.enemies`.
    std::optional<TargetMotion> FindTargetMotion(const Projectile& p, const ProjectileTargets& targets)
    {
        switch (p.target)
        {
        case ProjectileTarget::player:
            return TargetMotion{ targets.playerPosition, targets.playerVelocity };
        case ProjectileTarget::boss:
            // The boss is treated as still, so steering does not lead it.
            return TargetMotion{ targets.bossPosition, XMFLOAT3{} };
        case ProjectileTarget::enemy:
        {
            const auto found{ std::find_if(targets.enemies.begin(), targets.enemies.end(),
                [&p](const TrackedTarget& enemy) { return enemy.id == p.targetId; }) };
            if (found == targets.enemies.end()) return std::nullopt;
            return TargetMotion{ found->position, found->velocity };
        }
        case ProjectileTarget::none:
        default:
            return std::nullopt;
        }
    }

    // Turns `p` toward where its target will be when the projectile arrives.
    // Only the direction changes; the speed stays the same. Without a target it flies straight.
    void SteerTowardTarget(Projectile& p, float dt, const ProjectileTargets& targets)
    {
        XMVECTOR velocity{ XMLoadFloat3(&p.velocity) };
        const float speed{ XMVectorGetX(XMVector3Length(velocity)) };
        // Why: the lead time below divides by the speed, and a still projectile has no direction to turn.
        if (speed <= 0.0f) return;

        const std::optional<TargetMotion> motion{ FindTargetMotion(p, targets) };
        if (!motion) return;
        const XMFLOAT3 targetPosition{ motion->position };
        const XMFLOAT3 targetVelocity{ motion->velocity };

        const float dx{ targetPosition.x - p.position.x };
        const float dz{ targetPosition.z - p.position.z };
        const float distance{ std::sqrt((dx * dx) + (dz * dz)) };
        const float leadTime{ distance / speed };

        const XMFLOAT3 predicted{
            targetPosition.x + (targetVelocity.x * leadTime),
            targetPosition.y,
            targetPosition.z + (targetVelocity.z * leadTime),
        };
        const XMVECTOR toTarget{ XMVector3Normalize(
            XMVectorSubtract(XMLoadFloat3(&predicted), XMLoadFloat3(&p.position))) };

        float turnSpeed{ p.turnSpeed };
        if (distance < k_closeSteerDistance)
        {
            turnSpeed *= k_closeSteerDistance / (std::max)(distance, k_minSteerDistance);
        }

        velocity = XMVectorLerp(velocity, XMVectorScale(toTarget, speed), turnSpeed * dt);
        XMStoreFloat3(&p.velocity, XMVectorScale(XMVector3Normalize(velocity), speed));
    }

    // Keeps an attached effect on `p`, facing along the flight, or along yawDegrees while still.
    void FollowWithVfx(Projectile& p)
    {
        if (p.vfxHandle == -1) return;

        EffectManager& effects{ EffectManager::Instance() };
        if (!effects.IsPlaying(p.vfxHandle))
        {
            p.vfxHandle = -1;
            return;
        }

        const XMFLOAT3& v{ p.velocity };
        const float speed{ std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z)) };

        float pitch{ 0.0f };
        float yaw{ XMConvertToRadians(p.yawDegrees) };
        if (speed > k_minFacingSpeed)
        {
            const float horizontal{ std::sqrt((v.x * v.x) + (v.z * v.z)) };
            yaw = std::atan2(v.x, v.z);
            pitch = std::atan2(-v.y, horizontal);
        }
        // Why: the effect assets point backward relative to the direction of travel.
        yaw += XM_PI;

        const float scale{ p.visualScale * p.vfxScaleMultiplier };
        effects.SetPosition(p.vfxHandle, p.position);
        effects.SetRotation(p.vfxHandle, { pitch, yaw, 0.0f });
        effects.SetScale(p.vfxHandle, { scale, scale, scale });
    }
}

// Parentheses: braces would select the initializer-list constructor.
ProjectilePool::ProjectilePool(std::size_t capacity)
    : m_slots(capacity)
    , m_generations(capacity, 0u)
{
}

ProjectilePool::~ProjectilePool()
{
    Clear();
}

ProjectileHandle ProjectilePool::Spawn()
{
    for (std::size_t i{ 0 }; i < m_slots.size(); ++i)
    {
        if (m_slots[i].isActive) continue;

        m_slots[i] = Projectile{};
        m_slots[i].isActive = true;
        return HandleOf(i);
    }
    return ProjectileHandle{};
}

const Projectile* ProjectilePool::Get(ProjectileHandle handle) const
{
    if (handle.index >= m_slots.size()) return nullptr;
    if (m_generations[handle.index] != handle.generation) return nullptr;

    const Projectile& projectile{ m_slots[handle.index] };
    return projectile.isActive ? &projectile : nullptr;
}

Projectile* ProjectilePool::Get(ProjectileHandle handle)
{
    return const_cast<Projectile*>(static_cast<const ProjectilePool&>(*this).Get(handle));
}

ProjectileHandle ProjectilePool::HandleOf(std::size_t index) const
{
    assert(index < m_slots.size());
    return ProjectileHandle{ static_cast<std::uint32_t>(index), m_generations[index] };
}

void ProjectilePool::Despawn(std::size_t index)
{
    assert(index < m_slots.size());
    Projectile& projectile{ m_slots[index] };
    if (!projectile.isActive) return;

    StopVfx(index);
    projectile.isActive = false;
    ++m_generations[index];
}

void ProjectilePool::Despawn(ProjectileHandle handle)
{
    if (Get(handle)) Despawn(handle.index);
}

void ProjectilePool::Clear()
{
    for (std::size_t i{ 0 }; i < m_slots.size(); ++i)
    {
        Despawn(i);
    }
}

std::size_t ProjectilePool::CountActive() const
{
    return static_cast<std::size_t>(std::count_if(m_slots.begin(), m_slots.end(),
        [](const Projectile& projectile) { return projectile.isActive; }));
}

void ProjectilePool::DespawnOldest()
{
    std::size_t oldest{ m_slots.size() };
    for (std::size_t i{ 0 }; i < m_slots.size(); ++i)
    {
        if (!m_slots[i].isActive) continue;
        if ((oldest == m_slots.size()) || (m_slots[i].age > m_slots[oldest].age)) oldest = i;
    }
    if (oldest < m_slots.size()) Despawn(oldest);
}

void ProjectilePool::AttachVfx(std::size_t index, const char* path, float scale)
{
    assert(index < m_slots.size());
    StopVfx(index);

    Projectile& projectile{ m_slots[index] };
    projectile.vfxScaleMultiplier = scale;
    projectile.vfxHandle = EffectManager::Instance().Play(path, projectile.position, scale);
}

void ProjectilePool::StopVfx(std::size_t index)
{
    assert(index < m_slots.size());
    Projectile& projectile{ m_slots[index] };
    if (projectile.vfxHandle == -1) return;

    EffectManager::Instance().Stop(projectile.vfxHandle);
    projectile.vfxHandle = -1;
}

void ProjectilePool::Update(float dt, const ProjectileTargets& targets)
{
    for (Projectile& p : m_slots)
    {
        if (!p.isActive) continue;

        p.age += dt;

        if (p.isParabolic && p.target == ProjectileTarget::boss)
        {
            // Curved projectiles carry no attached effect, so there is nothing to follow.
            AdvanceAlongParabola(p, dt, targets.bossPosition);
            continue;
        }

        if (p.target != ProjectileTarget::none) SteerTowardTarget(p, dt, targets);

        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
        p.position.z += p.velocity.z * dt;

        FollowWithVfx(p);
    }
}

void ProjectilePool::Launch(Projectile& projectile, const XMFLOAT3& position,
    const XMFLOAT3& direction, float speed)
{
    projectile.position = position;
    XMStoreFloat3(&projectile.velocity,
        XMVectorScale(XMVector3Normalize(XMLoadFloat3(&direction)), speed));
}