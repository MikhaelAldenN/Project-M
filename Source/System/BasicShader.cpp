#include "Misc.h"
#include "GpuResourceUtils.h"
#include "BasicShader.h"

BasicShader::BasicShader(ID3D11Device* device)
{
	// 頂点シェーダー
	GpuResourceUtils::LoadVertexShader(
		device,
		"Data/Shader/BasicVS.cso",
		ModelResource::InputElementDescs.data(),
		static_cast<UINT>(ModelResource::InputElementDescs.size()),
		inputLayout.GetAddressOf(),
		vertexShader.GetAddressOf());

	// ピクセルシェーダー
	GpuResourceUtils::LoadPixelShader(
		device,
		"Data/Shader/BasicPS.cso",
		pixelShader.GetAddressOf());

	// マテリアル用定数バッファ
	GpuResourceUtils::CreateConstantBuffer(
		device,
		sizeof(CbMaterial),
		constantBuffer.GetAddressOf());
}

// 開始処理
void BasicShader::Begin(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定
	dc->IASetInputLayout(inputLayout.Get());
	dc->VSSetShader(vertexShader.Get(), nullptr, 0);
	dc->PSSetShader(pixelShader.Get(), nullptr, 0);

	// 定数バッファ設定
	ID3D11Buffer* cbs[] =
	{
		constantBuffer.Get(),
	};
	dc->PSSetConstantBuffers(1, _countof(cbs), cbs);
}

// 更新処理
void BasicShader::Update(const RenderContext& rc, const ModelResource::Material& material)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// マテリアル用定数バッファ更新
	CbMaterial cbMaterial{};
	cbMaterial.materialColor = material.color;
	dc->UpdateSubresource(constantBuffer.Get(), 0, 0, &cbMaterial, 0, 0);

	// シェーダーリソースビュー設定
	ID3D11ShaderResourceView* srvs[] =
	{
		material.shaderResourceView.Get(),
	};
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

// 描画終了
void BasicShader::End(const RenderContext& rc)
{
	ID3D11DeviceContext* dc = rc.deviceContext;

	// シェーダー設定解除
	dc->VSSetShader(nullptr, nullptr, 0);
	dc->PSSetShader(nullptr, nullptr, 0);
	dc->IASetInputLayout(nullptr);

	// 定数バッファ設定解除
	ID3D11Buffer* cbs[] = { nullptr };
	dc->PSSetConstantBuffers(1, _countof(cbs), cbs);

	// シェーダーリソースビュー設定解除
	ID3D11ShaderResourceView* srvs[] = { nullptr };
	dc->PSSetShaderResources(0, _countof(srvs), srvs);
}
