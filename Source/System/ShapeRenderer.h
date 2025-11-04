#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "RenderContext.h"

class ShapeRenderer
{
public:
	ShapeRenderer(ID3D11Device* device);
	~ShapeRenderer() {}

	// 箱描画
	void RenderBox(
		const RenderContext& rc,
		const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& angle,
		const DirectX::XMFLOAT3& size,
		const DirectX::XMFLOAT4& color) const;

	// 球描画
	void RenderSphere(
		const RenderContext& rc,
		const DirectX::XMFLOAT3& position,
		float radius,
		const DirectX::XMFLOAT4& color) const;

	// 円柱描画
	void RenderCylinder(
		const RenderContext& rc,
		const DirectX::XMFLOAT3& position,
		float radius,
		float height,
		const DirectX::XMFLOAT4& color) const;

	// カプセル描画
	void RenderCapsule(
		const RenderContext& rc,
		const DirectX::XMFLOAT4X4& transform,
		float radius,
		float height,
		const DirectX::XMFLOAT4& color) const;

private:
	struct Mesh
	{
		Microsoft::WRL::ComPtr<ID3D11Buffer>	vertexBuffer;
		UINT									vertexCount;
	};

	struct CbMesh
	{
		DirectX::XMFLOAT4X4		worldViewProjection;
		DirectX::XMFLOAT4		color;
	};
	// 描画実行
	void Render(const RenderContext& rc, const Mesh& mesh, const DirectX::XMFLOAT4X4& transform, const DirectX::XMFLOAT4& color) const;

	// メッシュ生成
	void CreateMesh(ID3D11Device* device, const std::vector<DirectX::XMFLOAT3>& vertices, Mesh& mesh);

	// 箱メッシュ作成
	void CreateBoxMesh(ID3D11Device* device, float width, float height, float depth);

	// 球メッシュ作成
	void CreateSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 半球メッシュ作成
	void CreateHalfSphereMesh(ID3D11Device* device, float radius, int subdivisions);

	// 円柱
	void CreateCylinderMesh(ID3D11Device* device, float radius1, float radius2, float start, float height, int subdivisions);

private:
	Mesh										boxMesh;
	Mesh										sphereMesh;
	Mesh										halfSphereMesh;
	Mesh										cylinderMesh;
	Microsoft::WRL::ComPtr<ID3D11VertexShader>	vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>	pixelShader;
	Microsoft::WRL::ComPtr<ID3D11InputLayout>	inputLayout;
	Microsoft::WRL::ComPtr<ID3D11Buffer>		constantBuffer;
};
