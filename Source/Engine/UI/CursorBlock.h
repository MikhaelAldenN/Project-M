#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "Primitive.h"

class Mouse;

class CursorBlock
{
public:
    CursorBlock(ID3D11Device* device);
    ~CursorBlock();

    // Sets the cursor size
    void Initialize(float width, float height);

    // Reads the cursor position from Input's Mouse.
    void Update(float dt, Mouse& mouse);
    void Update(float dt, float rawX, float rawY);

    void Render(ID3D11DeviceContext* context, Primitive* primitiveBatcher);

    // Retro options
    void SetGridSnap(bool enable, float gridW, float gridH);
    void SetBlink(bool enable, float visibleTime = 0.8f, float invisibleTime = 0.2f);
    void SetSize(float w, float h);

    void SetColor(float r, float g, float b, float a = 1.0f);

private:
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_invertBlendState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_oldBlendState;

    float m_width = 20.0f;
    float m_height = 20.0f;
    float m_posX = 0.0f;
    float m_posY = 0.0f;

    bool m_useGridSnap = false;
    float m_gridW = 20.0f;
    float m_gridH = 20.0f;

    bool m_useBlink = true;
    float m_blinkTimer = 0.0f;
    float m_blinkVisibleTime = 0.5f;   // seconds shown
    float m_blinkInvisibleTime = 0.2f; // seconds hidden
    bool m_isVisible = true;

    float m_color[4] = { 0.96f, 0.80f, 0.23f, 1.0f };
};