#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <sstream>
#include "Primitive.h"
#include "UIButtonPrimitive.h"
#include "BitmapFont.h"
#include "ResourceManager.h"

// Panel colors and sizes
struct PanelStyle {
    DirectX::XMFLOAT4 colorBg = { 0.0f, 0.0f, 0.0f, 1.0f };       // black
    DirectX::XMFLOAT4 colorBorder = { 0.96f, 0.80f, 0.23f, 1.0f };    // yellow
    DirectX::XMFLOAT4 colorText = { 0.96f, 0.80f, 0.23f, 1.0f };    // yellow
    DirectX::XMFLOAT4 colorDimmer = { 0.0f, 0.0f, 0.0f, 0.2f };       // dims the screen behind the panel
    float borderThickness = 2.5f;
    float textScale = 0.625f;
};

class UIPanel
{
public:
    UIPanel(Primitive* prim, float x, float y, float w, float h, const std::string& title);

    void SetStyle(const PanelStyle& newStyle) { m_style = newStyle; }
    void Show();
    void Hide();
    bool IsVisible() const { return m_isVisible; }

    void SetMessage(const std::string& msg); // one line per '\n'; no word wrap
    void AddButton(const std::string& label, float relX, float relY, float w, float h, std::function<void()> onClick);
    void SetOnClose(std::function<void()> callback) { m_onClose = callback; }

    void Update();
    void Render(ID3D11DeviceContext* dc);

private:
    void RecalculateTextLayout(); // splits rawMessage into formattedLines

    Primitive* m_primitive;
    float m_x, m_y, m_width, m_height;
    std::string m_title;

    // Text handling
    std::string m_rawMessage;
    std::vector<std::string> m_formattedLines;

    bool m_isVisible = false;
    PanelStyle m_style;
    std::vector<std::unique_ptr<UIButtonPrimitive>> m_buttons;
    std::function<void()> m_onClose = nullptr;
};