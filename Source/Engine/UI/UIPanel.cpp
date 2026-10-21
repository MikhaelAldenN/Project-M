#include "UIPanel.h"
#include "System/Input.h"
#include <sstream>

UIPanel::UIPanel(Primitive* prim, float x, float y, float w, float h, const std::string& title)
    : primitive(prim), x(x), y(y), width(w), height(h), title(title)
{
    // Default Style
    style = PanelStyle();
}

void UIPanel::Show() {
    isVisible = true;
}

void UIPanel::Hide() {
    isVisible = false;
    if (onClose) onClose();
}

void UIPanel::SetMessage(const std::string& msg)
{
    rawMessage = msg;
    RecalculateTextLayout();
}

void UIPanel::RecalculateTextLayout()
{
    formattedLines.clear();

    // One line per '\n'; long lines are not wrapped.
    std::stringstream ss(rawMessage);
    std::string segment;

    while (std::getline(ss, segment, '\n'))
    {
        formattedLines.push_back(segment);
    }
}

void UIPanel::AddButton(const std::string& label, float relX, float relY, float w, float h, std::function<void()> onClick)
{
    auto btn = std::make_unique<UIButtonPrimitive>(primitive, label, x + relX, y + relY, w, h);
    btn->SetOnClick(onClick);
    btn->SetAlignment(TextAlignment::Center);

    // Button styles
    DirectX::XMFLOAT4 transparent = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 blue = { 0.0f, 0.0f, 0.8f, 1.0f };

    // Standby: transparent, text in the panel border color
    btn->SetStyle(ButtonState::STANDBY, { transparent, transparent, style.colorBorder });

    // Hover: blue, white text
    btn->SetStyle(ButtonState::HOVER,   { blue, blue, white });

    // Pressed: solid border color, black text
    btn->SetStyle(ButtonState::PRESSED, { style.colorBorder, style.colorBorder, {0.0f, 0.0f, 0.0f, 1.0f} });

    buttons.push_back(std::move(btn));
}

void UIPanel::Update()
{
    if (!isVisible) return;
    for (auto& btn : buttons) btn->Update();
}

void UIPanel::Render(ID3D11DeviceContext* dc)
{
    if (!isVisible) return;

    // =========================================================
    // 1. BOX AND BORDER
    // =========================================================

    // A. Dim the whole screen
    primitive->Rect(0, 0, 1920, 1080, 0.0f, 0.0f, 0.0f,
        style.colorDimmer.x, style.colorDimmer.y, style.colorDimmer.z, style.colorDimmer.w);

    // B. Panel background, slightly larger than the border
    primitive->Rect(x - 8.0f, y - 8.0f, width + 16.0f, height + 16.0f,
        0.0f, 0.0f, 0.0f,
        style.colorBg.x, style.colorBg.y, style.colorBg.z, style.colorBg.w);

    // C. Border color
    primitive->Rect(x, y, width, height,
        0.0f, 0.0f, 0.0f,
        style.colorBorder.x, style.colorBorder.y, style.colorBorder.z, style.colorBorder.w);

    // D. Inner fill, leaving a thin border line
    float th = style.borderThickness;
    primitive->Rect(x + th, y + th, width - (th * 2), height - (th * 2),
        0.0f, 0.0f, 0.0f,
        style.colorBg.x, style.colorBg.y, style.colorBg.z, style.colorBg.w);

    // =========================================================
    // 2. HEADER BLOCKER
    // =========================================================
    // Covers the border line where the title sits.

    BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
    if (font)
    {
        DirectX::XMFLOAT2 titleSize = font->MeasureText(title, style.textScale);

        // +25 matches the title x offset below
        float labelPad = 8.0f;
        float blockerX = (x + 25.0f) - labelPad;
        float blockerY = y - (th * 2.0f);
        float blockerW = titleSize.x + (labelPad * 2.0f);
        float blockerH = th * 4.0f; // tall enough to cover the line

        primitive->Rect(blockerX, blockerY, blockerW, blockerH,
            0.0f, 0.0f, 0.0f,
            style.colorBg.x, style.colorBg.y, style.colorBg.z, style.colorBg.w);
    }

    // Flush the boxes before drawing text
    primitive->Render(dc);

    // =========================================================
    // 3. TEXT
    // =========================================================

    if (font)
    {
        // A. Title (top left, offset 25 px)
        font->Draw(title.c_str(), x + 25.0f, y - 8.0f, style.textScale,
            style.colorText.x, style.colorText.y, style.colorText.z, style.colorText.w);

        // B. Body text, centered
        float startY = y + 80.0f; // from the panel top

        for (const auto& line : formattedLines)
        {
            DirectX::XMFLOAT2 lineSize = font->MeasureText(line, style.textScale);

            // X = panel left + half the free space
            float lineX = x + (width - lineSize.x) / 2.0f;

            font->Draw(line.c_str(), lineX, startY, style.textScale,
                style.colorText.x, style.colorText.y, style.colorText.z, style.colorText.w);

            startY += 25.0f; // next line
        }
    }

    // =========================================================
    // 4. BUTTONS
    // =========================================================
    for (auto& btn : buttons)
    {
        btn->Render(dc, nullptr);
    }
}