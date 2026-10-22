#include "UIPanel.h"
#include "System/Input.h"
#include <sstream>

UIPanel::UIPanel(Primitive* prim, float x, float y, float w, float h, const std::string& title)
    : m_primitive(prim), m_x(x), m_y(y), m_width(w), m_height(h), m_title(title)
{
    // Default Style
    m_style = PanelStyle();
}

void UIPanel::Show() {
    m_isVisible = true;
}

void UIPanel::Hide() {
    m_isVisible = false;
    if (m_onClose) m_onClose();
}

void UIPanel::SetMessage(const std::string& msg)
{
    m_rawMessage = msg;
    RecalculateTextLayout();
}

void UIPanel::RecalculateTextLayout()
{
    m_formattedLines.clear();

    // One line per '\n'; long lines are not wrapped.
    std::stringstream ss(m_rawMessage);
    std::string segment;

    while (std::getline(ss, segment, '\n'))
    {
        m_formattedLines.push_back(segment);
    }
}

void UIPanel::AddButton(const std::string& label, float relX, float relY, float w, float h, std::function<void()> onClick)
{
    auto btn = std::make_unique<UIButtonPrimitive>(m_primitive, label, m_x + relX, m_y + relY, w, h);
    btn->SetOnClick(onClick);
    btn->SetAlignment(TextAlignment::Center);

    // Button styles
    DirectX::XMFLOAT4 transparent = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
    DirectX::XMFLOAT4 blue = { 0.0f, 0.0f, 0.8f, 1.0f };

    // Standby: transparent, text in the panel border color
    btn->SetStyle(ButtonState::STANDBY, { transparent, transparent, m_style.colorBorder });

    // Hover: blue, white text
    btn->SetStyle(ButtonState::HOVER,   { blue, blue, white });

    // Pressed: solid border color, black text
    btn->SetStyle(ButtonState::PRESSED, { m_style.colorBorder, m_style.colorBorder, {0.0f, 0.0f, 0.0f, 1.0f} });

    m_buttons.push_back(std::move(btn));
}

void UIPanel::Update()
{
    if (!m_isVisible) return;
    for (auto& btn : m_buttons) btn->Update();
}

void UIPanel::Render(ID3D11DeviceContext* dc)
{
    if (!m_isVisible) return;

    // =========================================================
    // 1. BOX AND BORDER
    // =========================================================

    // A. Dim the whole screen
    m_primitive->Rect(0, 0, 1920, 1080, 0.0f, 0.0f, 0.0f,
        m_style.colorDimmer.x, m_style.colorDimmer.y, m_style.colorDimmer.z, m_style.colorDimmer.w);

    // B. Panel background, slightly larger than the border
    m_primitive->Rect(m_x - 8.0f, m_y - 8.0f, m_width + 16.0f, m_height + 16.0f,
        0.0f, 0.0f, 0.0f,
        m_style.colorBg.x, m_style.colorBg.y, m_style.colorBg.z, m_style.colorBg.w);

    // C. Border color
    m_primitive->Rect(m_x, m_y, m_width, m_height,
        0.0f, 0.0f, 0.0f,
        m_style.colorBorder.x, m_style.colorBorder.y, m_style.colorBorder.z, m_style.colorBorder.w);

    // D. Inner fill, leaving a thin border line
    float th = m_style.borderThickness;
    m_primitive->Rect(m_x + th, m_y + th, m_width - (th * 2), m_height - (th * 2),
        0.0f, 0.0f, 0.0f,
        m_style.colorBg.x, m_style.colorBg.y, m_style.colorBg.z, m_style.colorBg.w);

    // =========================================================
    // 2. HEADER BLOCKER
    // =========================================================
    // Covers the border line where the title sits.

    BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
    if (font)
    {
        DirectX::XMFLOAT2 titleSize = font->MeasureText(m_title, m_style.textScale);

        // +25 matches the title x offset below
        float labelPad = 8.0f;
        float blockerX = (m_x + 25.0f) - labelPad;
        float blockerY = m_y - (th * 2.0f);
        float blockerW = titleSize.x + (labelPad * 2.0f);
        float blockerH = th * 4.0f; // tall enough to cover the line

        m_primitive->Rect(blockerX, blockerY, blockerW, blockerH,
            0.0f, 0.0f, 0.0f,
            m_style.colorBg.x, m_style.colorBg.y, m_style.colorBg.z, m_style.colorBg.w);
    }

    // Flush the boxes before drawing text
    m_primitive->Render(dc);

    // =========================================================
    // 3. TEXT
    // =========================================================

    if (font)
    {
        // A. Title (top left, offset 25 px)
        font->Draw(m_title.c_str(), m_x + 25.0f, m_y - 8.0f, m_style.textScale,
            m_style.colorText.x, m_style.colorText.y, m_style.colorText.z, m_style.colorText.w);

        // B. Body text, centered
        float startY = m_y + 80.0f; // from the panel top

        for (const auto& line : m_formattedLines)
        {
            DirectX::XMFLOAT2 lineSize = font->MeasureText(line, m_style.textScale);

            // X = panel left + half the free space
            float lineX = m_x + (m_width - lineSize.x) / 2.0f;

            font->Draw(line.c_str(), lineX, startY, m_style.textScale,
                m_style.colorText.x, m_style.colorText.y, m_style.colorText.z, m_style.colorText.w);

            startY += 25.0f; // next line
        }
    }

    // =========================================================
    // 4. BUTTONS
    // =========================================================
    for (auto& btn : m_buttons)
    {
        btn->Render(dc, nullptr);
    }
}