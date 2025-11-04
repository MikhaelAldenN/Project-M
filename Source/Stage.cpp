#include "Stage.h"

// =============================================================
// Stage::Stage/Destructor: Load & cleanup model
Stage::Stage()
{
    model = new Model("Data/Model/Stage/ExampleStage.mdl");
}
Stage::~Stage()
{
    delete model;
}

// =============================================================
// Stage::Update: Custom stage logic (placeholder)
void Stage::Update(float elapsedTime)
{
    // Update logic for the stage can be added here
}

// =============================================================
// Stage::Render: Render the stage model
void Stage::Render(const RenderContext& rc, ModelRenderer* renderer)
{
    DirectX::XMFLOAT4X4 transform;
    DirectX::XMStoreFloat4x4(&transform, DirectX::XMMatrixIdentity());
    renderer->Render(rc, transform, model, ShaderId::Lambert);
}