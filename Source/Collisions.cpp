#include "Collisions.h"

// =================================================================
// IntersectSphereVsSphere: Checks sphere-sphere collision
// Returns true if they intersect and adjusts positionB
// =================================================================
bool Collision::IntersectSphereVsSphere(const DirectX::XMFLOAT3& positionA, float radiusA, const DirectX::XMFLOAT3& positionB, float radiusB, DirectX::XMFLOAT3& outPositionB)
{
    //A->Bの単位ベクトルの算出
    DirectX::XMVECTOR PositionA = DirectX::XMLoadFloat3(&positionA);
    DirectX::XMVECTOR PositionB = DirectX::XMLoadFloat3(&positionB);
    DirectX::XMVECTOR Vec = DirectX::XMVectorSubtract(PositionB, PositionA);
    DirectX::XMVECTOR LenghtSq = DirectX::XMVector3LengthSq(Vec);
    float lenghtSq;
    DirectX::XMStoreFloat(&lenghtSq, LenghtSq);

    //距離判定
    float range = radiusA + radiusB;
    if (lenghtSq > range * range)
    {
        return false;
    }

    //AがBを押し出す
    DirectX::XMVECTOR n_vec = DirectX::XMVector3Normalize(Vec);
    DirectX::XMVECTOR s_vec = DirectX::XMVectorScale(n_vec, range - sqrtf(lenghtSq));
    DirectX::XMVECTOR c_vec = DirectX::XMVectorAdd(PositionB, s_vec);
    DirectX::XMStoreFloat3(&outPositionB, c_vec);
    return true;
}


// =================================================================
// IntersectSphereVsCylinder: Checks sphere-cylinder collision
// Returns true if they intersect and adjusts position
// =================================================================
bool Collision::IntersectSphereVsCylinder(const DirectX::XMFLOAT3& spherePosition, float sphereRadius, const DirectX::XMFLOAT3& cylinderPosiiton, float cylinderRadius, float cylinderHeight, DirectX::XMFLOAT3& outCylinderPosition)
{
	float heightSphere = spherePosition.y + sphereRadius;
	//Aの足元がBの頭より上なら当たってない
	if (cylinderPosiiton.y > cylinderPosiiton.y + heightSphere)
	{
		return false;
	}
	//Aの頭がBの足元より下なら当っていない
	if (spherePosition.y > cylinderPosiiton.y + cylinderHeight)
	{
		return false;
	}
	float vx = spherePosition.x - cylinderPosiiton.x;
	float vz = spherePosition.z - cylinderPosiiton.z;
	float range = sphereRadius + cylinderRadius;
	float distXZ = sqrtf(vx * vx + vz * vz);
	if (distXZ >= range)
	{
		return false;
	}
	//単位ベクトル化
	vx /= distXZ;
	vz /= distXZ;

	outCylinderPosition.x = spherePosition.x - vx * range;
	outCylinderPosition.z = spherePosition.z - vz * range;

	outCylinderPosition.y = cylinderPosiiton.y;


	return true;
}