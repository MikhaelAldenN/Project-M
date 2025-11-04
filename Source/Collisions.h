#pragma once

#include <DirectXMath.h>

/// =============================================================
/// Collision
/// - Provides static methods for collision detection
/// =============================================================
class Collision
{
public:
    // =================================================================
    // Sphere vs Sphere Collision
    // =================================================================
    static bool IntersectSphereVsSphere(
        const DirectX::XMFLOAT3& positionA,
        float radiusA,
        const DirectX::XMFLOAT3& positionB,
        float radiusB,
        DirectX::XMFLOAT3& outPositionB
    );

    // =================================================================
    // Sphere vs Cylinder Collision
    // =================================================================
    static bool IntersectSphereVsCylinder(
        const DirectX::XMFLOAT3& spherePosition,
        float sphereRadius,
        const DirectX::XMFLOAT3& cylinderPosition,
        float cylinderRadius,
        float cylinderHeight,
        DirectX::XMFLOAT3& outCylinderPosition
    );
};