#pragma once
#include "UIButton.h"
#include "Primitive.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <string>

enum class TextAlignment {
    Center,
    Left,
    Right
};

// Colors for one button state
struct ButtonStyle {
    DirectX::XMFLOAT4 backColor;   // RGBA; alpha 0 hides the background
    DirectX::XMFLOAT4 borderColor; // RGBA
    DirectX::XMFLOAT4 textColor;   // RGBA
};

class UIButtonPrimitive : public UIButton
{
public:
    UIButtonPrimitive(Primitive* primRenderer, const std::string& text, float x, float y, float w, float h)
        : UIButton(x, y, w, h), primitive(primRenderer), labelText(text)
    {
        // Default Settings
        paddingX = 10.0f;
        alignment = TextAlignment::Center;
        verticalAdjustment = 3.0f;
        textScale = 0.625f;

        // Default styles until SetStyle is called:
        // standby dark blue, hover bright blue, pressed green
        styles[0] = { {0.0f, 0.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }; // Standby
        styles[1] = { {0.0f, 0.0f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f} }; // Hover
        styles[2] = { {0.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f} }; // Pressed
    
        visibleChars = (int)text.length();
    }

    void SetVisibleChars(int count) {
        if (count < 0) count = 0;
        if (count > (int)labelText.length()) count = (int)labelText.length();
        visibleChars = count;
    }

    void SetState(ButtonState state) {
        currentState = state;
    }

    int GetVisibleChars() const { return visibleChars; }
    int GetTotalChars() const { return (int)labelText.length(); }
    bool IsFinishedTyping() const { return visibleChars >= (int)labelText.length(); }

    void SetStyle(ButtonState state, const ButtonStyle& style)
    {
        int index = static_cast<int>(state);
        if (index >= 0 && index < 3) {
            styles[index] = style;
        }
    }

    // Changes only the text color of one state
    void SetTextColor(ButtonState state, DirectX::XMFLOAT4 color) {
        int index = static_cast<int>(state);
        styles[index].textColor = color;
    }

    // --- Layout Setters ---
    void SetTextScale(float s) { textScale = s; }
    void SetAlignment(TextAlignment align) { alignment = align; }
    void SetPadding(float p) { paddingX = p; }
    void SetVerticalAdjustment(float y) { verticalAdjustment = y; }

    // --- Getters for ImGui / debug (mutable on purpose) ---
    const std::string& GetText() const { return labelText; }
    float& GetTextScale() { return textScale; }
    float& GetPadding() { return paddingX; }
    float& GetVerticalAdjustment() { return verticalAdjustment; }
    TextAlignment& GetAlignment() { return alignment; }
    ButtonStyle& GetStyle(ButtonState state) { return styles[static_cast<int>(state)]; }

    void Render(ID3D11DeviceContext* dc, Camera* camera) override
    {
        if (!primitive) return;

        // 1. Style of the current state
        int stateIdx = static_cast<int>(currentState);
        const ButtonStyle& activeStyle = styles[stateIdx];

        // 2. Background and border
        if (activeStyle.backColor.w > 0.0f) // skip a fully transparent background
        {
            primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
                activeStyle.backColor.x, activeStyle.backColor.y, activeStyle.backColor.z, activeStyle.backColor.w);
        }

        // Border
        primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
            activeStyle.borderColor.x, activeStyle.borderColor.y, activeStyle.borderColor.z, activeStyle.borderColor.w);

        primitive->Render(dc);

        // 3. Text
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font && visibleChars > 0)
        {
            // Measure the full text, so the anchor does not move while the text types out
            DirectX::XMFLOAT2 fullTextSize = font->MeasureText(labelText, textScale);

            // The visible part of the text
            std::string drawText = labelText.substr(0, visibleChars);

            // X from the full text width
            float textX = 0.0f;
            switch (alignment)
            {
            case TextAlignment::Left:
                textX = (float)rect.left + paddingX;
                break;
            case TextAlignment::Right:
                textX = (float)rect.left + width - fullTextSize.x - paddingX;
                break;
            case TextAlignment::Center:
            default:
                textX = (float)rect.left + (width - fullTextSize.x) / 2.0f;
                break;
            }

            float pressOffset = (currentState == ButtonState::PRESSED) ? 2.0f : 0.0f;
            float textY = (float)rect.top + (height - fullTextSize.y) / 2.0f + verticalAdjustment + pressOffset;

            // Draw the visible part at the full-text anchor
            font->Draw(drawText.c_str(), textX, textY, textScale,
                activeStyle.textColor.x, activeStyle.textColor.y, activeStyle.textColor.z, activeStyle.textColor.w);
        }
    }

private:
    Primitive* primitive;
    std::string labelText;

    // Indexed by ButtonState: STANDBY, HOVER, PRESSED
    ButtonStyle styles[3];

    int visibleChars = 0;

    float textScale;
    float paddingX;
    TextAlignment alignment;
    float verticalAdjustment;
};