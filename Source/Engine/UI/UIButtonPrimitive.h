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
        : UIButton(x, y, w, h), m_primitive(primRenderer), m_labelText(text)
    {
        // Default Settings
        m_paddingX = 10.0f;
        m_alignment = TextAlignment::Center;
        m_verticalAdjustment = 3.0f;
        m_textScale = 0.625f;

        // Default styles until SetStyle is called:
        // standby dark blue, hover bright blue, pressed green
        m_styles[0] = { {0.0f, 0.0f, 0.5f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f} }; // Standby
        m_styles[1] = { {0.0f, 0.0f, 0.8f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 1.0f} }; // Hover
        m_styles[2] = { {0.0f, 0.5f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.0f} }; // Pressed
    
        m_visibleChars = (int)text.length();
    }

    void SetVisibleChars(int count) {
        if (count < 0) count = 0;
        if (count > (int)m_labelText.length()) count = (int)m_labelText.length();
        m_visibleChars = count;
    }

    void SetState(ButtonState state) {
        currentState = state;
    }

    int GetVisibleChars() const { return m_visibleChars; }
    int GetTotalChars() const { return (int)m_labelText.length(); }
    bool IsFinishedTyping() const { return m_visibleChars >= (int)m_labelText.length(); }

    void SetStyle(ButtonState state, const ButtonStyle& style)
    {
        int index = static_cast<int>(state);
        if (index >= 0 && index < 3) {
            m_styles[index] = style;
        }
    }

    // Changes only the text color of one state
    void SetTextColor(ButtonState state, DirectX::XMFLOAT4 color) {
        int index = static_cast<int>(state);
        m_styles[index].textColor = color;
    }

    // --- Layout Setters ---
    void SetTextScale(float s) { m_textScale = s; }
    void SetAlignment(TextAlignment align) { m_alignment = align; }
    void SetPadding(float p) { m_paddingX = p; }
    void SetVerticalAdjustment(float y) { m_verticalAdjustment = y; }

    // --- Getters for ImGui / debug (mutable on purpose) ---
    const std::string& GetText() const { return m_labelText; }
    float& GetTextScale() { return m_textScale; }
    float& GetPadding() { return m_paddingX; }
    float& GetVerticalAdjustment() { return m_verticalAdjustment; }
    TextAlignment& GetAlignment() { return m_alignment; }
    ButtonStyle& GetStyle(ButtonState state) { return m_styles[static_cast<int>(state)]; }

    void Render(ID3D11DeviceContext* dc, Camera* camera) override
    {
        if (!m_primitive) return;

        // 1. Style of the current state
        int stateIdx = static_cast<int>(currentState);
        const ButtonStyle& activeStyle = m_styles[stateIdx];

        // 2. Background and border
        if (activeStyle.backColor.w > 0.0f) // skip a fully transparent background
        {
            m_primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
                activeStyle.backColor.x, activeStyle.backColor.y, activeStyle.backColor.z, activeStyle.backColor.w);
        }

        // Border
        m_primitive->Rect((float)rect.left, (float)rect.top, width, height, 0, 0, 0.0f,
            activeStyle.borderColor.x, activeStyle.borderColor.y, activeStyle.borderColor.z, activeStyle.borderColor.w);

        m_primitive->Render(dc);

        // 3. Text
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font && m_visibleChars > 0)
        {
            // Measure the full text, so the anchor does not move while the text types out
            DirectX::XMFLOAT2 fullTextSize = font->MeasureText(m_labelText, m_textScale);

            // The visible part of the text
            std::string drawText = m_labelText.substr(0, m_visibleChars);

            // X from the full text width
            float textX = 0.0f;
            switch (m_alignment)
            {
            case TextAlignment::Left:
                textX = (float)rect.left + m_paddingX;
                break;
            case TextAlignment::Right:
                textX = (float)rect.left + width - fullTextSize.x - m_paddingX;
                break;
            case TextAlignment::Center:
            default:
                textX = (float)rect.left + (width - fullTextSize.x) / 2.0f;
                break;
            }

            float pressOffset = (currentState == ButtonState::PRESSED) ? 2.0f : 0.0f;
            float textY = (float)rect.top + (height - fullTextSize.y) / 2.0f + m_verticalAdjustment + pressOffset;

            // Draw the visible part at the full-text anchor
            font->Draw(drawText.c_str(), textX, textY, m_textScale,
                activeStyle.textColor.x, activeStyle.textColor.y, activeStyle.textColor.z, activeStyle.textColor.w);
        }
    }

private:
    Primitive* m_primitive;
    std::string m_labelText;

    // Indexed by ButtonState: STANDBY, HOVER, PRESSED
    ButtonStyle m_styles[3];

    int m_visibleChars = 0;

    float m_textScale;
    float m_paddingX;
    TextAlignment m_alignment;
    float m_verticalAdjustment;
};