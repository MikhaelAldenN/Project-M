#include "StageCollider.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include "System/Model.h"

using namespace DirectX;

namespace
{
    // Closest point on triangle abc to point p (Ericson, Real-Time Collision Detection, 5.1.5).
    XMVECTOR ClosestPointOnTriangle(const XMVECTOR p, const XMVECTOR a, const XMVECTOR b, const XMVECTOR c)
    {
        const XMVECTOR ab{ XMVectorSubtract(b, a) };
        const XMVECTOR ac{ XMVectorSubtract(c, a) };
        const XMVECTOR ap{ XMVectorSubtract(p, a) };
        const float d1{ XMVectorGetX(XMVector3Dot(ab, ap)) };
        const float d2{ XMVectorGetX(XMVector3Dot(ac, ap)) };
        if (d1 <= 0.0f && d2 <= 0.0f) return a;

        const XMVECTOR bp{ XMVectorSubtract(p, b) };
        const float d3{ XMVectorGetX(XMVector3Dot(ab, bp)) };
        const float d4{ XMVectorGetX(XMVector3Dot(ac, bp)) };
        if (d3 >= 0.0f && d4 <= d3) return b;

        const float vc{ d1 * d4 - d3 * d2 };
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        {
            return XMVectorAdd(a, XMVectorScale(ab, d1 / (d1 - d3)));
        }

        const XMVECTOR cp{ XMVectorSubtract(p, c) };
        const float d5{ XMVectorGetX(XMVector3Dot(ab, cp)) };
        const float d6{ XMVectorGetX(XMVector3Dot(ac, cp)) };
        if (d6 >= 0.0f && d5 <= d6) return c;

        const float vb{ d5 * d2 - d1 * d6 };
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        {
            return XMVectorAdd(a, XMVectorScale(ac, d2 / (d2 - d6)));
        }

        const float va{ d3 * d6 - d5 * d4 };
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        {
            return XMVectorAdd(b, XMVectorScale(XMVectorSubtract(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
        }

        const float denominator{ 1.0f / (va + vb + vc) };
        return XMVectorAdd(a, XMVectorAdd(XMVectorScale(ab, vb * denominator), XMVectorScale(ac, vc * denominator)));
    }

    // Whether p, lying on the plane of triangle abc with unit normal, is inside the triangle.
    // Points within k_insideTolerance of an edge count as inside, so a touch on the edge shared
    // by two triangles is not lost to rounding in both.
    bool IsInsideTriangle(const XMVECTOR p, const XMVECTOR a, const XMVECTOR b, const XMVECTOR c, const XMVECTOR normal)
    {
        constexpr float k_insideTolerance{ 1.0e-4f };
        const XMVECTOR corners[3]{ a, b, c };
        for (int i = 0; i < 3; ++i)
        {
            const XMVECTOR edge{ XMVectorSubtract(corners[(i + 1) % 3], corners[i]) };
            const XMVECTOR toPoint{ XMVectorSubtract(p, corners[i]) };
            // The cross product's length along the normal is the edge length times the signed
            // distance of p from the edge, positive on the inner side.
            const float side{ XMVectorGetX(XMVector3Dot(XMVector3Cross(edge, toPoint), normal)) };
            if (side < -k_insideTolerance * XMVectorGetX(XMVector3Length(edge))) return false;
        }
        return true;
    }

    // Earliest t in [0, maxT] at which a point moving from start along unit direction comes
    // within radius of center; false when it never does.
    bool RayVsSphere(const XMVECTOR start, const XMVECTOR direction, const XMVECTOR center,
        float radius, float maxT, float& outT)
    {
        const XMVECTOR m{ XMVectorSubtract(start, center) };
        const float b{ XMVectorGetX(XMVector3Dot(m, direction)) };
        const float c{ XMVectorGetX(XMVector3Dot(m, m)) - radius * radius };
        if (c > 0.0f && b > 0.0f) return false;

        const float discriminant{ b * b - c };
        if (discriminant < 0.0f) return false;

        const float t{ (std::max)(-b - std::sqrt(discriminant), 0.0f) };
        if (t > maxT) return false;
        outT = t;
        return true;
    }

    // Earliest t in [0, maxT] at which a point moving from start along unit direction comes
    // within radius of the inside of segment pq, ignoring its end caps (the vertex tests cover them).
    bool RayVsEdgeCylinder(const XMVECTOR start, const XMVECTOR direction, const XMVECTOR p,
        const XMVECTOR q, float radius, float maxT, float& outT)
    {
        const XMVECTOR edge{ XMVectorSubtract(q, p) };
        const XMVECTOR m{ XMVectorSubtract(start, p) };
        const float edgeLengthSq{ XMVectorGetX(XMVector3Dot(edge, edge)) };
        const float md{ XMVectorGetX(XMVector3Dot(m, edge)) };
        const float nd{ XMVectorGetX(XMVector3Dot(direction, edge)) };
        const float mn{ XMVectorGetX(XMVector3Dot(m, direction)) };

        // Solve |(m + t d) - edge * s(t)|^2 = r^2, where s(t) projects onto the edge's line.
        const float a{ edgeLengthSq - nd * nd };
        if (a < 1.0e-8f * edgeLengthSq) return false;  // Moving along the edge: only the caps can be hit

        const float k{ XMVectorGetX(XMVector3Dot(m, m)) - radius * radius };
        const float b{ edgeLengthSq * mn - nd * md };
        const float c{ edgeLengthSq * k - md * md };
        const float discriminant{ b * b - a * c };
        if (discriminant < 0.0f) return false;

        const float t{ (-b - std::sqrt(discriminant)) / a };
        if (t < 0.0f || t > maxT) return false;

        const float alongEdge{ md + t * nd };
        if (alongEdge < 0.0f || alongEdge > edgeLengthSq) return false;
        outT = t;
        return true;
    }

    // Sweeps a sphere of radius from start along unit direction for up to maxDistance against
    // triangle abc. Only the front face is solid: the side cross(b - a, c - a) points to.
    // Reports the distance the center travels before touching, and the triangle's front normal.
    bool SweepSphereVsTriangle(const XMVECTOR start, const XMVECTOR direction, float maxDistance, float radius,
        const XMVECTOR a, const XMVECTOR b, const XMVECTOR c, float& outDistance, XMVECTOR& outNormal)
    {
        const XMVECTOR normal{ XMVector3Normalize(XMVector3Cross(XMVectorSubtract(b, a), XMVectorSubtract(c, a))) };
        const float approach{ XMVectorGetX(XMVector3Dot(direction, normal)) };
        const float startHeight{ XMVectorGetX(XMVector3Dot(XMVectorSubtract(start, a), normal)) };

        // Moving parallel to or away from the front face, or starting behind it: never a hit.
        if (approach >= 0.0f || startHeight < 0.0f) return false;

        // Already touching: stop at once so the caller cannot slide further into the face.
        const XMVECTOR closestAtStart{ ClosestPointOnTriangle(start, a, b, c) };
        if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(start, closestAtStart))) <= radius * radius)
        {
            outDistance = 0.0f;
            outNormal = normal;
            return true;
        }

        // Face interior first: if the sphere meets the plane inside the triangle, nothing touches earlier.
        if (startHeight > radius)
        {
            const float tPlane{ (startHeight - radius) / -approach };
            if (tPlane > maxDistance) return false;

            const XMVECTOR centerAtPlane{ XMVectorMultiplyAdd(direction, XMVectorReplicate(tPlane), start) };
            const XMVECTOR touch{ XMVectorSubtract(centerAtPlane, XMVectorScale(normal, radius)) };
            if (IsInsideTriangle(touch, a, b, c, normal))
            {
                outDistance = tPlane;
                outNormal = normal;
                return true;
            }
        }

        // Otherwise the first contact is on an edge or a corner. A contact made while the center
        // is already behind the plane comes from the back side, which is not solid.
        const auto inFront = [&](float t)
            {
                return startHeight + approach * t >= 0.0f;
            };

        float earliest{ maxDistance };
        bool hit{ false };
        const XMVECTOR corners[3]{ a, b, c };
        for (int i = 0; i < 3; ++i)
        {
            float t{ 0.0f };
            if (RayVsSphere(start, direction, corners[i], radius, earliest, t) && inFront(t))
            {
                earliest = t;
                hit = true;
            }
            if (RayVsEdgeCylinder(start, direction, corners[i], corners[(i + 1) % 3], radius, earliest, t) && inFront(t))
            {
                earliest = t;
                hit = true;
            }
        }

        if (!hit) return false;
        outDistance = earliest;
        outNormal = normal;
        return true;
    }
}

void StageCollider::Build(const Model& model, const std::vector<WallBox>& walls)
{
    m_triangles.clear();

    for (const Model::Mesh& mesh : model.GetMeshes())
    {
        if (mesh.node == nullptr) continue;

        const XMMATRIX world{ XMLoadFloat4x4(&mesh.node->worldTransform) };
        const std::size_t vertexCount{ mesh.vertices.size() };

        for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
        {
            Triangle triangle{};
            bool indicesValid{ true };
            for (std::size_t corner = 0; corner < 3; ++corner)
            {
                const std::uint32_t vertexIndex{ mesh.indices[i + corner] };
                if (vertexIndex >= vertexCount)
                {
                    indicesValid = false;
                    break;
                }
                const XMVECTOR local{ XMLoadFloat3(&mesh.vertices[vertexIndex].position) };
                XMStoreFloat3(&triangle[corner], XMVector3TransformCoord(local, world));
            }
            if (indicesValid) AddTriangle(triangle);
        }
    }

    for (const WallBox& wall : walls)
    {
        AddWallBox(wall);
    }

    BuildCells();
}

void StageCollider::AddTriangle(const Triangle& triangle)
{
    // Why skipped: a zero-area triangle has no normal, so a cast against it cannot report one.
    const XMVECTOR a{ XMLoadFloat3(&triangle[0]) };
    const XMVECTOR edgeAB{ XMVectorSubtract(XMLoadFloat3(&triangle[1]), a) };
    const XMVECTOR edgeAC{ XMVectorSubtract(XMLoadFloat3(&triangle[2]), a) };
    if (XMVectorGetX(XMVector3LengthSq(XMVector3Cross(edgeAB, edgeAC))) < 1.0e-12f) return;

    m_triangles.push_back(triangle);
}

void StageCollider::AddWallBox(const WallBox& wall)
{
    // Corner i takes -halfExtents or +halfExtents on each axis from bits 0 (X), 1 (Y), 2 (Z).
    const XMMATRIX world{ XMLoadFloat4x4(&wall.world) };
    std::array<XMFLOAT3, 8> corners{};
    for (std::size_t i = 0; i < corners.size(); ++i)
    {
        const XMVECTOR local{ XMVectorSet(
            (i & 1u) ? wall.halfExtents.x : -wall.halfExtents.x,
            (i & 2u) ? wall.halfExtents.y : -wall.halfExtents.y,
            (i & 4u) ? wall.halfExtents.z : -wall.halfExtents.z,
            1.0f) };
        XMStoreFloat3(&corners[i], XMVector3TransformCoord(local, world));
    }

    // Each face as four corner indices going around its edge.
    constexpr std::array<std::array<std::size_t, 4>, 6> k_faces{ {
        { 0, 1, 3, 2 }, { 4, 6, 7, 5 },  // -Z, +Z
        { 0, 4, 5, 1 }, { 2, 3, 7, 6 },  // -Y, +Y
        { 0, 2, 6, 4 }, { 1, 5, 7, 3 },  // -X, +X
    } };

    // Why the winding is checked: the casts hit only the side the triangle's normal
    // (cross of its first two edges) faces, which must point away from the box.
    const XMVECTOR center{ XMVector3TransformCoord(XMVectorZero(), world) };
    const auto addOutward = [this, center](const XMFLOAT3& p0, const XMFLOAT3& p1, const XMFLOAT3& p2)
        {
            const XMVECTOR a{ XMLoadFloat3(&p0) };
            const XMVECTOR normal{ XMVector3Cross(XMVectorSubtract(XMLoadFloat3(&p1), a), XMVectorSubtract(XMLoadFloat3(&p2), a)) };
            const bool facesOut{ XMVectorGetX(XMVector3Dot(normal, XMVectorSubtract(a, center))) >= 0.0f };
            AddTriangle(facesOut ? Triangle{ p0, p1, p2 } : Triangle{ p0, p2, p1 });
        };

    for (const auto& face : k_faces)
    {
        addOutward(corners[face[0]], corners[face[1]], corners[face[2]]);
        addOutward(corners[face[0]], corners[face[2]], corners[face[3]]);
    }
}

bool StageCollider::SphereCast(const XMFLOAT3& origin, const XMFLOAT3& direction,
    float radius, float distance, SweepHit& outHit)
{
    if (m_triangles.empty() || distance <= 0.0f) return false;

    const XMVECTOR start{ XMLoadFloat3(&origin) };
    const XMVECTOR castDirection{ XMVector3Normalize(XMLoadFloat3(&direction)) };
    const XMVECTOR end{ XMVectorMultiplyAdd(castDirection, XMVectorReplicate(distance), start) };
    XMFLOAT3 endPoint{};
    XMStoreFloat3(&endPoint, end);

    CollectCandidates(GetCellRange(
        (std::min)(origin.x, endPoint.x) - radius, (std::min)(origin.z, endPoint.z) - radius,
        (std::max)(origin.x, endPoint.x) + radius, (std::max)(origin.z, endPoint.z) + radius));

    bool hasHit{ false };
    float closestDistance{ (std::numeric_limits<float>::max)() };

    for (const std::uint32_t triangleIndex : m_candidates)
    {
        const Triangle& triangle{ m_triangles[triangleIndex] };
        const XMVECTOR corners[3]{
            XMLoadFloat3(&triangle[0]),
            XMLoadFloat3(&triangle[1]),
            XMLoadFloat3(&triangle[2])
        };

        float hitDistance{ 0.0f };
        XMVECTOR hitNormal{};
        if (!SweepSphereVsTriangle(start, castDirection, distance, radius, corners[0], corners[1], corners[2], hitDistance, hitNormal)) continue;
        if (hitDistance >= closestDistance) continue;

        closestDistance = hitDistance;
        const XMVECTOR centerAtContact{ XMVectorMultiplyAdd(castDirection, XMVectorReplicate(hitDistance), start) };
        XMStoreFloat3(&outHit.position, ClosestPointOnTriangle(centerAtContact, corners[0], corners[1], corners[2]));
        XMStoreFloat3(&outHit.normal, hitNormal);
        outHit.distance = hitDistance;
        hasHit = true;
    }

    return hasHit;
}

void StageCollider::BuildCells()
{
    m_cellStart.clear();
    m_cellTriangles.clear();
    m_columns = 0;
    m_rows = 0;
    m_candidates.clear();
    m_queryStampPerTriangle.assign(m_triangles.size(), 0);
    m_queryStamp = 0;

    if (m_triangles.empty()) return;

    float minX{ (std::numeric_limits<float>::max)() };
    float minZ{ (std::numeric_limits<float>::max)() };
    float maxX{ std::numeric_limits<float>::lowest() };
    float maxZ{ std::numeric_limits<float>::lowest() };
    for (const Triangle& triangle : m_triangles)
    {
        for (const XMFLOAT3& corner : triangle)
        {
            minX = (std::min)(minX, corner.x);
            minZ = (std::min)(minZ, corner.z);
            maxX = (std::max)(maxX, corner.x);
            maxZ = (std::max)(maxZ, corner.z);
        }
    }

    m_gridMinX = minX;
    m_gridMinZ = minZ;
    m_columns = static_cast<int>((maxX - minX) / k_cellSize) + 1;
    m_rows = static_cast<int>((maxZ - minZ) / k_cellSize) + 1;

    const auto triangleRange = [this](const Triangle& triangle)
        {
            return GetCellRange(
                (std::min)({ triangle[0].x, triangle[1].x, triangle[2].x }),
                (std::min)({ triangle[0].z, triangle[1].z, triangle[2].z }),
                (std::max)({ triangle[0].x, triangle[1].x, triangle[2].x }),
                (std::max)({ triangle[0].z, triangle[1].z, triangle[2].z }));
        };

    // Two passes: count triangles per cell, then write them into their slots.
    m_cellStart.assign(static_cast<std::size_t>(m_columns) * m_rows + 1, 0);
    for (const Triangle& triangle : m_triangles)
    {
        const CellRange range{ triangleRange(triangle) };
        for (int row = range.firstRow; row <= range.lastRow; ++row)
        {
            for (int column = range.firstColumn; column <= range.lastColumn; ++column)
            {
                ++m_cellStart[static_cast<std::size_t>(row) * m_columns + column + 1];
            }
        }
    }
    std::partial_sum(m_cellStart.begin(), m_cellStart.end(), m_cellStart.begin());

    m_cellTriangles.resize(m_cellStart.back());
    std::vector<std::uint32_t> writeCursor(m_cellStart.begin(), m_cellStart.end() - 1);
    for (std::size_t triangleIndex = 0; triangleIndex < m_triangles.size(); ++triangleIndex)
    {
        const CellRange range{ triangleRange(m_triangles[triangleIndex]) };
        for (int row = range.firstRow; row <= range.lastRow; ++row)
        {
            for (int column = range.firstColumn; column <= range.lastColumn; ++column)
            {
                const std::size_t cell{ static_cast<std::size_t>(row) * m_columns + column };
                m_cellTriangles[writeCursor[cell]++] = static_cast<std::uint32_t>(triangleIndex);
            }
        }
    }
}

StageCollider::CellRange StageCollider::GetCellRange(float minX, float minZ, float maxX, float maxZ) const noexcept
{
    const float gridMaxX{ m_gridMinX + static_cast<float>(m_columns) * k_cellSize };
    const float gridMaxZ{ m_gridMinZ + static_cast<float>(m_rows) * k_cellSize };
    if (m_columns == 0 || maxX < m_gridMinX || maxZ < m_gridMinZ || minX >= gridMaxX || minZ >= gridMaxZ)
    {
        return {};
    }

    const auto toIndex = [](float value, float gridMin, int count)
        {
            const int index{ static_cast<int>(std::floor((value - gridMin) / k_cellSize)) };
            return std::clamp(index, 0, count - 1);
        };

    return CellRange{
        toIndex(minX, m_gridMinX, m_columns),
        toIndex(minZ, m_gridMinZ, m_rows),
        toIndex(maxX, m_gridMinX, m_columns),
        toIndex(maxZ, m_gridMinZ, m_rows)
    };
}

void StageCollider::CollectCandidates(const CellRange& range)
{
    m_candidates.clear();

    // A triangle spanning several cells is collected once: it is skipped when its stamp
    // already equals the stamp of this query.
    ++m_queryStamp;
    if (m_queryStamp == 0)
    {
        std::fill(m_queryStampPerTriangle.begin(), m_queryStampPerTriangle.end(), 0u);
        m_queryStamp = 1;
    }

    for (int row = range.firstRow; row <= range.lastRow; ++row)
    {
        for (int column = range.firstColumn; column <= range.lastColumn; ++column)
        {
            const std::size_t cell{ static_cast<std::size_t>(row) * m_columns + column };
            for (std::uint32_t slot = m_cellStart[cell]; slot < m_cellStart[cell + 1]; ++slot)
            {
                const std::uint32_t triangleIndex{ m_cellTriangles[slot] };
                if (m_queryStampPerTriangle[triangleIndex] == m_queryStamp) continue;

                m_queryStampPerTriangle[triangleIndex] = m_queryStamp;
                m_candidates.push_back(triangleIndex);
            }
        }
    }
}