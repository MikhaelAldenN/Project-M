#pragma once

#include <DirectXMath.h>
#include "Model.h"

// コリジョン
class Collision
{
public:
	// レイキャスト
	static bool RayCast(
		const DirectX::XMFLOAT3& start,
		const DirectX::XMFLOAT3& end,
		const DirectX::XMFLOAT4X4& worldTransform,
		const Model* model,
		DirectX::XMFLOAT3& hitPosition,
		DirectX::XMFLOAT3& hitNormal);
};
