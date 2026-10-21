#include "ProjectileMesh.h"

#include <algorithm>
#include <cassert>
#include <vector>
#include "System/Graphics.h"

using namespace DirectX;

void ProjectileMesh::Load(const char* path)
{
    m_model = std::make_shared<Model>(Graphics::Instance().GetDevice(), path);

    // Why: each draw passes its own world matrix, which replaces the mesh node's world
    // transform. Capture that node's offset from the root once, with the root at identity.
    Model::Node* root{ m_model->GetRootNode() };
    root->position = { 0.0f, 0.0f, 0.0f };
    root->rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    root->scale = { 1.0f, 1.0f, 1.0f };

    XMFLOAT4X4 identity{};
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_model->UpdateTransform(identity);

    const std::vector<Model::Mesh>& meshes{ m_model->GetMeshes() };
    assert(!meshes.empty() && "projectile model has no mesh");
    // One world matrix per draw only works when every mesh hangs off the same node.
    assert(std::all_of(meshes.begin(), meshes.end(),
        [&](const Model::Mesh& mesh) { return mesh.node == meshes.front().node; }));
    m_meshOffset = meshes.front().node->worldTransform;
}

void ProjectileMesh::Draw(ModelRenderer& renderer, ShaderId shader, const XMFLOAT4& color,
    const XMFLOAT3& position, float scale, float yawRadians) const
{
    assert(m_model && "ProjectileMesh::Draw called before Load");

    // Same composition Model::UpdateTransform applies to the root: scale, rotation, translation.
    const XMMATRIX world{ XMLoadFloat4x4(&m_meshOffset)
        * XMMatrixScaling(scale, scale, scale)
        * XMMatrixRotationY(yawRadians)
        * XMMatrixTranslation(position.x, position.y, position.z) };
    XMFLOAT4X4 worldMatrix{};
    XMStoreFloat4x4(&worldMatrix, world);

    renderer.Draw(shader, m_model, color, worldMatrix);
}