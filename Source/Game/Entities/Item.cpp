#include "Item.h"
#include "System/Graphics.h"

using namespace DirectX;

Item::Item(ID3D11Device* device, const DirectX::XMFLOAT3& position, ItemType type)
{
    // Reuse Block model for memory optimization
    m_model = std::make_shared<Model>(device, "Data/Model/Character/PLACEHOLDER_mdl_Block.glb");

    m_type = type;
    m_movement->SetPosition(position);
    m_originalY = position.y;
    m_animTime = 0.0f;
    m_scale = { 2.0f, 2.0f, 2.0f };

    // Initialize color from type
    SetType(type);
}

void Item::Update(float elapsedTime, Camera* camera)
{
    if (!IsActive()) return;

    m_animTime += elapsedTime;

    // Floating animation (sine wave on Y axis)
    XMFLOAT3 pos = m_movement->GetPosition();
    pos.y = m_originalY + sinf(m_animTime * kFloatSpeed) * kFloatAmp;
    m_movement->SetPosition(pos);

    // Spinning animation
    m_movement->SetRotation({ 0.0f, m_animTime * kSpinSpeed, 0.0f });

    SyncData();
}

void Item::Render(ModelRenderer* renderer)
{
    if (!IsActive()) return;

    renderer->Draw(ShaderId::Phong, m_model, m_color);
}