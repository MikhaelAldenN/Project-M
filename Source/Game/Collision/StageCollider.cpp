#include "StageCollider.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <SphereCast.h>
#include "System/Model.h"

using namespace DirectX;

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
    const XMVECTOR end{ XMVectorMultiplyAdd(XMLoadFloat3(&direction), XMVectorReplicate(distance), start) };
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

        SphereCastResult result{};
        if (!IntersectSphereCastVsTriangle(start, end, radius, corners, &result)) continue;
        if (result.distance >= closestDistance) continue;

        closestDistance = result.distance;
        XMStoreFloat3(&outHit.position, result.position);
        XMStoreFloat3(&outHit.normal, result.normal);
        outHit.distance = result.distance;
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