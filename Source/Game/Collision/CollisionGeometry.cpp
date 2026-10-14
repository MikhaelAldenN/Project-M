#include "CollisionGeometry.h"

#include <cmath>
#include <utility>
#include "Stage.h"

using namespace DirectX;

namespace CollisionGeometry
{
    float RayCastOBB(XMVECTOR rayOrigin, XMVECTOR rayDir, float rayLength, float radius, const DebugWallData& wall, XMVECTOR& outNormal)
    {
        XMVECTOR vWallPos = XMLoadFloat3(&wall.Position);
        XMMATRIX matRot = XMMatrixRotationRollPitchYaw(
            XMConvertToRadians(wall.Rotation.x),
            XMConvertToRadians(wall.Rotation.y),
            XMConvertToRadians(wall.Rotation.z)
        );
        XMMATRIX matInvRot = XMMatrixTranspose(matRot);

        // Work in the wall's local frame, where the box is axis-aligned.
        XMVECTOR vRelOrigin = XMVectorSubtract(rayOrigin, vWallPos);
        XMVECTOR vLocalOrigin = XMVector3TransformNormal(vRelOrigin, matInvRot);
        XMVECTOR vLocalDir = XMVector3TransformNormal(rayDir, matInvRot);

        float r = radius;
        float minX = -wall.Scale.x - r; float maxX = wall.Scale.x + r;
        float minZ = -wall.Scale.z - r; float maxZ = wall.Scale.z + r;

        float tMin = 0.0f;
        float tMax = rayLength;

        XMFLOAT3 start, dir;
        XMStoreFloat3(&start, vLocalOrigin);
        XMStoreFloat3(&dir, vLocalDir);

        // Slab test on X, then Z. A ray parallel to a slab misses unless it starts inside it.
        if (std::abs(dir.x) < 1e-6f) {
            if (start.x < minX || start.x > maxX) return -1.0f;
        }
        else {
            float invD = 1.0f / dir.x;
            float t1 = (minX - start.x) * invD;
            float t2 = (maxX - start.x) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) return -1.0f;
        }

        if (std::abs(dir.z) < 1e-6f) {
            if (start.z < minZ || start.z > maxZ) return -1.0f;
        }
        else {
            float invD = 1.0f / dir.z;
            float t1 = (minZ - start.z) * invD;
            float t2 = (maxZ - start.z) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) return -1.0f;
        }

        // The face closest to the hit point gives the normal.
        XMFLOAT3 hitPoint;
        XMStoreFloat3(&hitPoint, vLocalOrigin + vLocalDir * tMin);

        float distMinX = std::abs(hitPoint.x - minX);
        float distMaxX = std::abs(hitPoint.x - maxX);
        float distMinZ = std::abs(hitPoint.z - minZ);
        float distMaxZ = std::abs(hitPoint.z - maxZ);

        float bestDist = distMinX;
        XMVECTOR localNormal = XMVectorSet(-1, 0, 0, 0);

        if (distMaxX < bestDist) { bestDist = distMaxX; localNormal = XMVectorSet(1, 0, 0, 0); }
        if (distMinZ < bestDist) { bestDist = distMinZ; localNormal = XMVectorSet(0, 0, -1, 0); }
        if (distMaxZ < bestDist) { bestDist = distMaxZ; localNormal = XMVectorSet(0, 0, 1, 0); }

        outNormal = XMVector3TransformNormal(localNormal, matRot);

        return tMin;
    }

    float DistancePointToLineSegment2D(const XMFLOAT3& A, const XMFLOAT3& B, const XMFLOAT3& P)
    {
        float lineX = B.x - A.x;
        float lineZ = B.z - A.z;
        float px = P.x - A.x;
        float pz = P.z - A.z;

        float lineLengthSq = (lineX * lineX) + (lineZ * lineZ);

        // Degenerate segment: plain point distance.
        if (lineLengthSq == 0.0f) return std::sqrt((px * px) + (pz * pz));

        // Project P onto AB and clamp to the segment.
        float t = ((px * lineX) + (pz * lineZ)) / lineLengthSq;
        t = (std::max)(0.0f, (std::min)(1.0f, t));

        float closestX = A.x + t * lineX;
        float closestZ = A.z + t * lineZ;

        float dx = P.x - closestX;
        float dz = P.z - closestZ;

        return std::sqrt((dx * dx) + (dz * dz));
    }
}