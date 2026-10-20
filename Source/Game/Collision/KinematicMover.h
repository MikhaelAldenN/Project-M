#pragma once

#include <DirectXMath.h>

class StageCollider;

// Moves a character on the XZ plane against a StageCollider with sphere casts; Y never changes.
// The swept sphere's bottom sits stepOffset above the feet, so geometry lower than stepOffset
// does not block. Every surface hit is treated as a vertical wall that the motion slides along.
// The whole move is swept in one cast per slide, so a fast move such as a dash cannot pass a wall.
namespace KinematicMover
{
    struct Settings
    {
        float radius{ 0.0f };             // Sphere radius on the XZ plane
        float stepOffset{ 0.0f };         // Geometry lower than this above the feet does not block
        float skinWidth{ 0.0f };          // Gap kept from every wall so the next cast starts outside it
        bool blockWhereNoFloor{ false };  // Cancels motion that would leave all geometry below
    };

    // Returns feet moved by the X and Z of displacement; displacement.y is ignored.
    [[nodiscard]] DirectX::XMFLOAT3 Move(StageCollider& collider, const DirectX::XMFLOAT3& feet,
        const DirectX::XMFLOAT3& displacement, const Settings& settings);

    // Finds the first ground straight below from, within maxDistance. Used to stand a character
    // on the floor at spawn. Returns false when nothing is below.
    [[nodiscard]] bool FindGroundHeight(StageCollider& collider, const DirectX::XMFLOAT3& from,
        float maxDistance, float& outGroundY);
}