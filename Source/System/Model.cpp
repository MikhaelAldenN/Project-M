#include "Graphics.h"
#include "Model.h"

// コンストラクタ
Model::Model(const char* filename)
{
	// リソース読み込み
	resource = std::make_shared<ModelResource>();
	resource->Load(Graphics::Instance().GetDevice(), filename);

	// ノード
	const std::vector<ModelResource::Node>& resNodes = resource->GetNodes();

	nodes.resize(resNodes.size());
	for (size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex)
	{
		auto&& src = resNodes.at(nodeIndex);
		auto&& dst = nodes.at(nodeIndex);

		dst.name = src.name.c_str();
		dst.parent = src.parentIndex >= 0 ? &nodes.at(src.parentIndex) : nullptr;
		dst.scale = src.scale;
		dst.rotate = src.rotate;
		dst.translate = src.translate;

		if (dst.parent != nullptr)
		{
			dst.parent->children.emplace_back(&dst);
		}
	}

	// 行列計算
	UpdateTransform();
}

// 変換行列計算
void Model::UpdateTransform()
{
	for (Node& node : nodes)
	{
		// ローカル行列算出
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotate));
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.translate.x, node.translate.y, node.translate.z);
		DirectX::XMMATRIX LocalTransform = S * R * T;
		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);

		// グローバル行列算出
		if (node.parent != nullptr)
		{
			DirectX::XMMATRIX ParentGlobalTransform = DirectX::XMLoadFloat4x4(&node.parent->globalTransform);
			DirectX::XMMATRIX GlobalTransform = DirectX::XMMatrixMultiply(LocalTransform, ParentGlobalTransform);
			DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
		}
		else
		{
			DirectX::XMStoreFloat4x4(&node.globalTransform, LocalTransform);
		}
	}
}
