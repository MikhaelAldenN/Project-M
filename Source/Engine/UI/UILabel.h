#pragma once
#include "Primitive.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <string>
#include <DirectXMath.h>

class UILabel
{
public:
    UILabel(Primitive* primRenderer) : m_primitive(primRenderer) {}

    void SetPosition(float x, float y) { this->m_x = x; this->m_y = y; }
    void SetColor(float r, float g, float b, float a) { m_textColor = { r, g, b, a }; }
    void SetBackgroundColor(float r, float g, float b, float a) { m_backColor = { r, g, b, a }; }
    void SetScale(float s) { m_scale = s; }
    void SetPadding(float p) { m_padding = p; }

    // Sets the text and re-measures it for the background
    void SetText(const std::string& newText)
    {
        m_text = newText;

        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            DirectX::XMFLOAT2 size = font->MeasureText(m_text, m_scale);
            m_textWidth = size.x;
            m_textHeight = size.y;
        }
    }

    void Render(ID3D11DeviceContext* dc)
    {
        if (m_text.empty() || !m_primitive) return;

        // 1. Background box, drawn over the panel border
        // text width plus padding on both sides, minus 7 px
        float bgWidth = m_textWidth + (m_padding * 2.0f) - 7;
        float bgHeight = m_textHeight;

        m_primitive->Rect(m_x, m_y, bgWidth, bgHeight, 0.0f, 0, 0.0f,
            m_backColor.x, m_backColor.y, m_backColor.z, m_backColor.w);

        m_primitive->Render(dc);

        // 2. Text on top
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            font->Draw(m_text.c_str(), m_x + m_padding, m_y, m_scale,
                m_textColor.x, m_textColor.y, m_textColor.z, m_textColor.w);
        }
    }

private:
    Primitive* m_primitive = nullptr;
    std::string m_text;

    float m_x = 0.0f, m_y = 0.0f;
    float m_textWidth = 0.0f;
    float m_textHeight = 0.0f;

    // Styling Default
    float m_scale = 0.625f;
    float m_padding = 4.0f; // gap between the text and the box edge

    DirectX::XMFLOAT4 m_textColor = { 0.96f, 0.80f, 0.23f, 1.0f }; // yellow
    DirectX::XMFLOAT4 m_backColor = { 0.0f, 0.0f, 0.0f, 1.0f };    // opaque black, so it hides the border behind it
};