#pragma once
#include "Primitive.h"
#include "ResourceManager.h"
#include "BitmapFont.h"
#include <string>
#include <DirectXMath.h>

class UILabel
{
public:
    UILabel(Primitive* primRenderer) : primitive(primRenderer) {}

    void SetPosition(float x, float y) { this->x = x; this->y = y; }
    void SetColor(float r, float g, float b, float a) { textColor = { r, g, b, a }; }
    void SetBackgroundColor(float r, float g, float b, float a) { backColor = { r, g, b, a }; }
    void SetScale(float s) { scale = s; }
    void SetPadding(float p) { padding = p; }

    // Sets the text and re-measures it for the background
    void SetText(const std::string& newText)
    {
        text = newText;

        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            DirectX::XMFLOAT2 size = font->MeasureText(text, scale);
            textWidth = size.x;
            textHeight = size.y;
        }
    }

    void Render(ID3D11DeviceContext* dc)
    {
        if (text.empty() || !primitive) return;

        // 1. Background box, drawn over the panel border
        // text width plus padding on both sides, minus 7 px
        float bgWidth = textWidth + (padding * 2.0f) - 7;
        float bgHeight = textHeight;

        primitive->Rect(x, y, bgWidth, bgHeight, 0.0f, 0, 0.0f,
            backColor.x, backColor.y, backColor.z, backColor.w);

        primitive->Render(dc);

        // 2. Text on top
        BitmapFont* font = ResourceManager::Instance().GetFont("VGA_FONT");
        if (font)
        {
            font->Draw(text.c_str(), x + padding, y, scale,
                textColor.x, textColor.y, textColor.z, textColor.w);
        }
    }

private:
    Primitive* primitive = nullptr;
    std::string text;

    float x = 0.0f, y = 0.0f;
    float textWidth = 0.0f;
    float textHeight = 0.0f;

    // Styling Default
    float scale = 0.625f;
    float padding = 4.0f; // gap between the text and the box edge

    DirectX::XMFLOAT4 textColor = { 0.96f, 0.80f, 0.23f, 1.0f }; // yellow
    DirectX::XMFLOAT4 backColor = { 0.0f, 0.0f, 0.0f, 1.0f };    // opaque black, so it hides the border behind it
};