#pragma once

#include <DirectXMath.h>
#include "RenderState.h"

struct RenderContext
{
	ID3D11DeviceContext*	deviceContext;
	const RenderState*		renderState = nullptr;
	DirectX::XMFLOAT4X4		view;
	DirectX::XMFLOAT4X4		projection;
	DirectX::XMFLOAT3		lightDirection = { 0, -1, 0 };
};
