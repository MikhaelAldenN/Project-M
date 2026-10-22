#include "Character.h"

using namespace DirectX;

Character::Character()
    : m_movement(std::make_unique<CharacterMovement>())
{
}

// ~Character() is compiler-generated — unique_ptr cleans up movement automatically

void Character::Render(ModelRenderer* renderer)
{
    if (m_model) renderer->Draw(ShaderId::Lambert, m_model);
}

void Character::RenderDebug(const RenderContext& rc, ShapeRenderer* renderer)
{
    if (!m_movement) return;

    XMFLOAT3 pos = m_movement->GetPosition();
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT3 rot = m_movement->GetRotation();
    XMMATRIX R = XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);

    XMFLOAT4X4 transform;
    XMStoreFloat4x4(&transform, R * T);

    renderer->DrawCapsule(transform, 0.5f, 1.6f, { 0, 1, 0, 1 });
}

void Character::SyncData()
{
    if (!m_model || m_model->GetNodes().empty()) return;

    Model::Node& rootNode = m_model->GetNodes().at(0);

    // Sync position, rotation, and scale from movement state to model root node
    rootNode.position = m_movement->GetPosition();

    XMFLOAT3 rot = m_movement->GetRotation();
    XMVECTOR qRot = XMQuaternionRotationRollPitchYaw(rot.x, rot.y, rot.z);
    XMStoreFloat4(&rootNode.rotation, qRot);

    rootNode.scale = m_scale;

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    m_model->UpdateTransform(identity);
}