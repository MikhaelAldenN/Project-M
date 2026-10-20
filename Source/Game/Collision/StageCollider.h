#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>

class Model;

// Static triangles of a stage model in world space, bucketed on the XZ plane,
// answering sphere casts. Replaces the PhysX triangle-mesh actor of the stage.
class StageCollider
{
public:
    struct SweepHit
    {
        DirectX::XMFLOAT3 position{};  // Contact point on the hit triangle
        DirectX::XMFLOAT3 normal{};    // Normal of the hit triangle
        float distance{ 0.0f };        // Distance from the cast origin to the sphere center at first contact
    };

    // Replaces all triangles with the triangles of model, transformed by each mesh node's
    // worldTransform. Call after Model::UpdateTransform, and again whenever the stage moves.
    void Build(const Model& model);

    // Sweeps a sphere of radius from origin along direction (unit length) for up to distance.
    // Returns the closest hit; false when the path is clear or nothing has been built.
    [[nodiscard]] bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction,
        float radius, float distance, SweepHit& outHit);

    [[nodiscard]] std::size_t GetTriangleCount() const noexcept { return m_triangles.size(); }

private:
    using Triangle = std::array<DirectX::XMFLOAT3, 3>;

    struct CellRange
    {
        int firstColumn{ 0 };
        int firstRow{ 0 };
        int lastColumn{ -1 };
        int lastRow{ -1 };
    };

    void BuildCells();
    [[nodiscard]] CellRange GetCellRange(float minX, float minZ, float maxX, float maxZ) const noexcept;
    void CollectCandidates(const CellRange& range);

    // Why 2 m: stage tiles are 4.5 m and props 1 to 4 m wide, so most triangles land in 1 to 4 cells.
    static constexpr float k_cellSize{ 2.0f };

    std::vector<Triangle> m_triangles{};

    // Triangle indices per XZ cell, stored contiguously: cell c owns
    // m_cellTriangles[m_cellStart[c]] up to (not including) m_cellTriangles[m_cellStart[c + 1]].
    std::vector<std::uint32_t> m_cellStart{};
    std::vector<std::uint32_t> m_cellTriangles{};
    float m_gridMinX{ 0.0f };
    float m_gridMinZ{ 0.0f };
    int m_columns{ 0 };
    int m_rows{ 0 };

    // Per-query scratch, kept between casts so a cast does not allocate once the buffers have grown.
    std::vector<std::uint32_t> m_candidates{};
    std::vector<std::uint32_t> m_queryStampPerTriangle{};
    std::uint32_t m_queryStamp{ 0 };
};