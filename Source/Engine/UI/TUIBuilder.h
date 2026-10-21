#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include "ButtonManager.h"
#include "UIButtonPrimitive.h"
#include "Primitive.h"

// Visual theme (colors and sizes) applied to every button the builder adds.
struct TUITheme {
    ButtonStyle styleStandby;
    ButtonStyle styleHover;
    ButtonStyle stylePress;

    float textScale = 0.625f;
    float padding = 10.0f;
    float verticalAdj = 2.0f;
    TextAlignment align = TextAlignment::Left;
};

class TUIBuilder
{
public:
    // Both pointers are borrowed: the manager stores the buttons, the batcher draws them.
    TUIBuilder(ButtonManager* manager, Primitive* batcher);
    ~TUIBuilder() = default;

    // 1. SETUP: start position and style
    void SetStartPosition(float x, float y);
    void SetSpacing(float spacing);
    void SetButtonSize(float width, float height);
    void SetTheme(const TUITheme& theme);

    // 2. BUILD: adds a button below the previous one.
    // Returns a borrowed pointer for scenes that need to reach the button later.
    UIButtonPrimitive* AddButton(const std::string& text, std::function<void()> onClick);

    // A label or decoration that cannot be clicked
    UIButtonPrimitive* AddDecoration(const std::string& text);

    // Moves the layout cursor back to the start (for example to begin a new column).
    void ResetCursor();

private:
    ButtonManager* m_manager;
    Primitive* m_batcher;

    // Layout State
    float m_startX = 0.0f;
    float m_startY = 0.0f;
    float m_currentY = 0.0f;

    float m_btnWidth = 200.0f;
    float m_btnHeight = 40.0f;
    float m_spacing = 5.0f;

    // Active Theme
    TUITheme m_theme;
};