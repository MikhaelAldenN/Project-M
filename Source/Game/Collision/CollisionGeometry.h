#pragma once

#include <algorithm>
#include <DirectXMath.h>

struct DebugWallData;

// Geometry tests used by the gameplay collision checks.
namespace CollisionGeometry
{
    // Axis-aligned box used for broad-phase rejection.
    struct AABB
    {
        DirectX::XMFLOAT3 minPoint{};
        DirectX::XMFLOAT3 maxPoint{};
    };

    [[nodiscard]] inline bool CheckAABBIntersection(const AABB& a, const AABB& b) noexcept
    {
        return (a.minPoint.x <= b.maxPoint.x && a.maxPoint.x >= b.minPoint.x) &&
            (a.minPoint.y <= b.maxPoint.y && a.maxPoint.y >= b.minPoint.y) &&
            (a.minPoint.z <= b.maxPoint.z && a.maxPoint.z >= b.minPoint.z);
    }

    // Box that encloses a sphere of the given radius moving from startPos to endPos.
    [[nodiscard]] inline AABB CreateSweptAABB(const DirectX::XMFLOAT3& startPos,
        const DirectX::XMFLOAT3& endPos,
        float radius) noexcept
    {
        return AABB{
            { (std::min)(startPos.x, endPos.x) - radius,
              (std::min)(startPos.y, endPos.y) - radius,
              (std::min)(startPos.z, endPos.z) - radius },
            { (std::max)(startPos.x, endPos.x) + radius,
              (std::max)(startPos.y, endPos.y) + radius,
              (std::max)(startPos.z, endPos.z) + radius }
        };
    }

    // Casts a ray against a wall box on its local X and Z axes; Y is ignored.
    // The box half-extents are wall.Scale.x and wall.Scale.z, inflated by radius.
    // Returns the ray parameter of the first hit in [0, rayLength], or -1 when nothing is hit.
    // On a hit, outNormal receives the world-space normal of the face that was hit.
    [[nodiscard]] float RayCastOBB(DirectX::XMVECTOR rayOrigin, DirectX::XMVECTOR rayDir, float rayLength,
        float radius, const DebugWallData& wall, DirectX::XMVECTOR& outNormal);

    // Shortest distance on the XZ plane from P to the segment AB. Returns the point distance when A == B.
    [[nodiscard]] float DistancePointToLineSegment2D(const DirectX::XMFLOAT3& A,
        const DirectX::XMFLOAT3& B,
        const DirectX::XMFLOAT3& P);
}