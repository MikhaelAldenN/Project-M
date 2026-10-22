#pragma once

#include <string>
#include <map>
#include <memory>
#include "System/Sprite.h"
#include <DirectXMath.h>

// One glyph from the .fnt file
struct CharData {
    int id;
    int x, y;
    int width, height;
    int xoffset, yoffset;
    int xadvance;
};

class BitmapFont
{
public:
    // Loads the font texture and parses the .fnt data
    BitmapFont(const std::string& texturePath, const std::string& fontDataPath);
    ~BitmapFont() = default;

    // Draws text in screen pixels
    void Draw(const std::string& text, float x, float y, float scale, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
    DirectX::XMFLOAT2 MeasureText(const std::string& text, float scale);

    void Draw3D(const std::string& text,
        const Camera* camera,
        DirectX::XMFLOAT3 worldPos,
        float scale,
        DirectX::XMFLOAT3 rotation = { 0,0,0 }, // Pitch, Yaw, Roll
        DirectX::XMFLOAT4 color = { 1,1,1,1 });

private:
    std::unique_ptr<Sprite> m_sprite;
    std::map<int, CharData> m_chars;  // glyphs by code point

    // Reads one integer field of an .fnt line
    int ParseValue(const std::string& line, const std::string& key);
};