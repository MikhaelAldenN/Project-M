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
        : rect{ (long)x, (long)y, (long)(x + width), (long)(y + height) }
        , width(width), height(height) {}

    virtual ~UIButton() = default;

    // --- Drawing (each subclass) ---
    virtual void Render(ID3D11DeviceContext* dc, class Camera* camera) = 0;

    // --- Shared input logic ---
    void Update()
    {
        // 1. A selected button always shows as PRESSED, even when the mouse is away.
        if (isSelected)
        {
            currentState = ButtonState::PRESSED;

            // Returning here keeps the hover logic below from overriding the state.
            return;
        }

        // 2. Mouse logic, only while not selected
        Mouse& mouse = Input::Instance().GetMouse();
        POINT mousePos = { mouse.GetPositionX(), mouse.GetPositionY() };

        bool isHover = PtInRect(&rect, mousePos);
        bool isMouseDown = mouse.GetButtonDown() & Mouse::BTN_LEFT;

        if (isHover)
        {
            if (isMouseDown) currentState = ButtonState::PRESSED;
            else
            {
                // A click fires on release inside the button.
                if (currentState == ButtonState::PRESSED && callback) callback();
                currentState = ButtonState::HOVER;
            }
        }
        else
        {
            currentState = ButtonState::STANDBY;
        }
    }

    // --- Selection and click callback ---
    void SetSelected(bool value) { isSelected = value; }
    bool IsSelected() const { return isSelected; }
    void SetOnClick(OnClickCallback cb) { callback = cb; }
    ButtonState GetState() const { return currentState; }

    float GetX() const { return (float)rect.left; }
    float GetY() const { return (float)rect.top; }
    float GetWidth() const { return width; }
    float GetHeight() const { return height; }
    
    void SetPosition(float x, float y)
    {
        rect.left = (long)x;
        rect.top = (long)y;
        rect.right = (long)(x + width);
        rect.bottom = (long)(y + height);
    }

    void SetSize(float w, float h)
    {
        width = w;
        height = h;
        rect.right = rect.left + (long)width;
        rect.bottom = rect.top + (long)height;
    }


protected:
    RECT rect;
    float width, height;
    ButtonState currentState = ButtonState::STANDBY;
    OnClickCallback callback;
    bool isSelected = false;
};