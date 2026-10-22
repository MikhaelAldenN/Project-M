#pragma once

#include <functional>
#include <DirectXMath.h>
#include <d3d11.h>
#include "System/Input.h"

// Callback type
using OnClickCallback = std::function<void()>;

enum class ButtonState { STANDBY, HOVER, PRESSED };

class UIButton
{
public:
    UIButton(float x, float y, float width, float height)
        : m_rect{ (long)x, (long)y, (long)(x + width), (long)(y + height) }
        , m_width(width), m_height(height) {}

    virtual ~UIButton() = default;

    // --- Drawing (each subclass) ---
    virtual void Render(ID3D11DeviceContext* dc, class Camera* camera) = 0;

    // --- Shared input logic ---
    void Update()
    {
        // 1. A selected button always shows as PRESSED, even when the mouse is away.
        if (m_isSelected)
        {
            m_currentState = ButtonState::PRESSED;

            // Returning here keeps the hover logic below from overriding the state.
            return;
        }

        // 2. Mouse logic, only while not selected
        Mouse& mouse = Input::Instance().GetMouse();
        POINT mousePos = { mouse.GetPositionX(), mouse.GetPositionY() };

        bool isHover = PtInRect(&m_rect, mousePos);
        bool isMouseDown = mouse.GetButtonDown() & Mouse::BTN_LEFT;

        if (isHover)
        {
            if (isMouseDown) m_currentState = ButtonState::PRESSED;
            else
            {
                // A click fires on release inside the button.
                if (m_currentState == ButtonState::PRESSED && m_callback) m_callback();
                m_currentState = ButtonState::HOVER;
            }
        }
        else
        {
            m_currentState = ButtonState::STANDBY;
        }
    }

    // --- Selection and click callback ---
    void SetSelected(bool value) { m_isSelected = value; }
    bool IsSelected() const { return m_isSelected; }
    void SetOnClick(OnClickCallback cb) { m_callback = cb; }
    ButtonState GetState() const { return m_currentState; }

    float GetX() const { return (float)m_rect.left; }
    float GetY() const { return (float)m_rect.top; }
    float GetWidth() const { return m_width; }
    float GetHeight() const { return m_height; }
    
    void SetPosition(float x, float y)
    {
        m_rect.left = (long)x;
        m_rect.top = (long)y;
        m_rect.right = (long)(x + m_width);
        m_rect.bottom = (long)(y + m_height);
    }

    void SetSize(float w, float h)
    {
        m_width = w;
        m_height = h;
        m_rect.right = m_rect.left + (long)m_width;
        m_rect.bottom = m_rect.top + (long)m_height;
    }


protected:
    RECT m_rect;
    float m_width, m_height;
    ButtonState m_currentState = ButtonState::STANDBY;
    OnClickCallback m_callback;
    bool m_isSelected = false;
};