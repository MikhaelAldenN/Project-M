#include "KinematicMover.h"

#include <algorithm>
#include "StageCollider.h"

using namespace DirectX;

namespace
{
    // One hit, then up to two corrections along the next walls.
    constexpr int k_maxSlideIterations{ 3 };
    constexpr float k_minMoveLength{ 1.0e-4f };

    // Why thin: the probes ask about the point under the feet, not anywhere under the character,
    // so the character stops at an edge instead of hanging half a radius past it.
    constexpr float k_groundProbeRadius{ 0.05f };

    // Why this far: ground in the stages never lies more than a few units below the
    // player, so a cast this long that finds none means there is no geometry below at all.
    constexpr float k_floorProbeDistance{ 100.0f };

    [[nodiscard]] bool HasGroundBelow(StageCollider& collider, const XMFLOAT3& point)
    {
        StageCollider::SweepHit hit{};
        return collider.SphereCast(point, { 0.0f, -1.0f, 0.0f }, k_groundProbeRadius, k_floorProbeDistance, hit);
    }
}

namespace KinematicMover
{
    XMFLOAT3 Move(StageCollider& collider, const XMFLOAT3& feet, const XMFLOAT3& displacement, const Settings& settings)
    {
        // The swept sphere is lifted so its bottom sits stepOffset above the feet.
        const XMVECTOR startCenter{ XMVectorSet(feet.x, feet.y + settings.stepOffset + settings.radius, feet.z, 0.0f) };
        XMVECTOR center{ startCenter };
        XMVECTOR move{ XMVectorSet(displacement.x, 0.0f, displacement.z, 0.0f) };

        for (int iteration = 0; iteration < k_maxSlideIterations; ++iteration)
        {
            const float moveLength{ XMVectorGetX(XMVector3Length(move)) };
            if (moveLength < k_minMoveLength) break;

            const XMVECTOR direction{ XMVectorScale(move, 1.0f / moveLength) };
            XMFLOAT3 origin{};
            XMFLOAT3 castDirection{};
            XMStoreFloat3(&origin, center);
            XMStoreFloat3(&castDirection, direction);

            // Why the cast is longer than the move: it finds walls the skin gap would otherwise hide.
            StageCollider::SweepHit hit{};
            if (!collider.SphereCast(origin, castDirection, settings.radius, moveLength + settings.skinWidth, hit))
            {
                center = XMVectorAdd(center, move);
                break;
            }

            // Stop skinWidth short of the contact so the next cast starts outside the wall.
            const float travel{ (std::max)(hit.distance - settings.skinWidth, 0.0f) };
            center = XMVectorAdd(center, XMVectorScale(direction, travel));

            // Why flattened: Y is fixed, so a sloped surface is slid along as if it were vertical.
            XMVECTOR wallNormal{ XMVectorSetY(XMLoadFloat3(&hit.normal), 0.0f) };
            if (XMVectorGetX(XMVector3LengthSq(wallNormal)) < k_minMoveLength) break;
            wallNormal = XMVector3Normalize(wallNormal);

            const XMVECTOR leftover{ XMVectorScale(direction, moveLength - travel) };
            move = XMVectorSubtract(leftover, XMVectorScale(wallNormal, XMVectorGetX(XMVector3Dot(leftover, wallNormal))));
        }

        XMFLOAT3 startPoint{};
        XMFLOAT3 endPoint{};
        XMStoreFloat3(&startPoint, startCenter);
        XMStoreFloat3(&endPoint, center);

        // Leaving all geometry is refused, but a character already outside may still walk back.
        if (settings.blockWhereNoFloor && !HasGroundBelow(collider, endPoint) && HasGroundBelow(collider, startPoint))
        {
            return feet;
        }

        return { endPoint.x, feet.y, endPoint.z };
    }

    bool FindGroundHeight(StageCollider& collider, const XMFLOAT3& from, float maxDistance, float& outGroundY)
    {
        StageCollider::SweepHit hit{};
        if (!collider.SphereCast(from, { 0.0f, -1.0f, 0.0f }, k_groundProbeRadius, maxDistance, hit)) return false;

        // The probe's center stops one probe radius above the ground it touches.
        outGroundY = from.y - hit.distance - k_groundProbeRadius;
        return true;
    }
}