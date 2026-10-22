#include "CursorBlock.h"
#include "System/Mouse.h"
#include <cmath>

CursorBlock::CursorBlock(ID3D11Device* device)
{
    // Standard alpha blend (not invert), so the cursor stays bright amber
    // on any background.

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    blendDesc.RenderTarget[0].BlendEnable = TRUE;

    // SRC_ALPHA: weight by the cursor alpha
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    // INV_SRC_ALPHA: the background shows through the rest
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    device->CreateBlendState(&blendDesc, m_invertBlendState.GetAddressOf());
}

CursorBlock::~CursorBlock() {}

void CursorBlock::Initialize(float w, float h)
{
    this->m_width = w;
    this->m_height = h;
}

void CursorBlock::SetGridSnap(bool enable, float gw, float gh)
{
    m_useGridSnap = enable;
    m_gridW = gw;
    m_gridH = gh;
}

void CursorBlock::SetBlink(bool enable, float visibleTime, float invisibleTime)
{
    m_useBlink = enable;
    m_blinkVisibleTime = visibleTime;
    m_blinkInvisibleTime = invisibleTime;

    // Restart visible, so a newly enabled blink begins with the cursor shown.
    m_isVisible = true;
    m_blinkTimer = 0.0f;
}

void CursorBlock::SetSize(float w, float h)
{
    this->m_width = w;
    this->m_height = h;
}

void CursorBlock::SetColor(float r, float g, float b, float a)
{
    m_color[0] = r; m_color[1] = g; m_color[2] = b; m_color[3] = a;
}

// Main update: takes the raw cursor position.
void CursorBlock::Update(float dt, float rawX, float rawY)
{
    // 1. Logic Snapping
    if (m_useGridSnap)
    {
        m_posX = floorf(rawX / m_gridW) * m_gridW;
        m_posY = floorf(rawY / m_gridH) * m_gridH;
    }
    else
    {
        m_posX = rawX;
        m_posY = rawY;
    }

    // 2. Logic Blinking
    if (m_useBlink)
    {
        m_blinkTimer += dt;
        float currentLimit = m_isVisible ? m_blinkVisibleTime : m_blinkInvisibleTime;

        if (m_blinkTimer >= currentLimit)
        {
            m_blinkTimer -= currentLimit;
            m_isVisible = !m_isVisible;
        }
    }
    else
    {
        m_isVisible = true;
    }
}

// Convenience overload: reads the position from the mouse and forwards it.
void CursorBlock::Update(float dt, Mouse& mouse)
{
    float mx = static_cast<float>(mouse.GetPositionX());
    float my = static_cast<float>(mouse.GetPositionY());

    Update(dt, mx, my);
}

void CursorBlock::Render(ID3D11DeviceContext* context, Primitive* primitiveBatcher)
{
    if (!m_isVisible || !primitiveBatcher) return;

    // 1. Save the current blend state
    float blendFactor[4];
    UINT sampleMask;
    context->OMGetBlendState(m_oldBlendState.GetAddressOf(), blendFactor, &sampleMask);

    // 2. Bind the cursor blend state (standard alpha, despite the member name)
    context->OMSetBlendState(m_invertBlendState.Get(), nullptr, 0xFFFFFFFF);

    // 3. Draw the block and flush the batch
    primitiveBatcher->Rect(m_posX, m_posY, m_width, m_height, 0.0f, 0.0f, 0.0f, m_color[0], m_color[1], m_color[2], m_color[3]);
    primitiveBatcher->Render(context);

    // 4. Restore the saved blend state
    context->OMSetBlendState(m_oldBlendState.Get(), blendFactor, sampleMask);
    m_oldBlendState.Reset();
}