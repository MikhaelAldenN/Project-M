#include "BitmapFont.h"
#include "System/Graphics.h"
#include "Camera.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace
{
    size_t NextUtf8Offset(const std::string& text, size_t offset)
    {
        if (offset >= text.size()) return text.size();

        const unsigned char lead = static_cast<unsigned char>(text[offset]);
        size_t length = 1;

        if ((lead & 0x80) == 0x00) length = 1;
        else if ((lead & 0xE0) == 0xC0) length = 2;
        else if ((lead & 0xF0) == 0xE0) length = 3;
        else if ((lead & 0xF8) == 0xF0) length = 4;

        return (std::min)(offset + length, text.size());
    }

    int DecodeUtf8Codepoint(const std::string& text, size_t offset, size_t nextOffset)
    {
        const unsigned char c0 = static_cast<unsigned char>(text[offset]);
        const size_t length = nextOffset - offset;

        if (length == 1) return c0;
        if (length == 2)
        {
            return ((c0 & 0x1F) << 6) |
                (static_cast<unsigned char>(text[offset + 1]) & 0x3F);
        }
        if (length == 3)
        {
            return ((c0 & 0x0F) << 12) |
                ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 6) |
                (static_cast<unsigned char>(text[offset + 2]) & 0x3F);
        }
        if (length == 4)
        {
            return ((c0 & 0x07) << 18) |
                ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 12) |
                ((static_cast<unsigned char>(text[offset + 2]) & 0x3F) << 6) |
                (static_cast<unsigned char>(text[offset + 3]) & 0x3F);
        }

        return c0;
    }
}

BitmapFont::BitmapFont(const std::string& texturePath, const std::string& fontDataPath)
{
    // 1. Sprite for the font texture
    auto device = Graphics::Instance().GetDevice();
    sprite = std::make_unique<Sprite>(device, texturePath.c_str());

    // 2. Parse file .fnt
    std::ifstream file(fontDataPath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        // Only "char id=..." lines define glyphs.
        if (line.find("char id=") != std::string::npos)
        {
            CharData c;
            c.id = ParseValue(line, "id");
            c.x = ParseValue(line, "x");
            c.y = ParseValue(line, "y");
            c.width = ParseValue(line, "width");
            c.height = ParseValue(line, "height");
            c.xoffset = ParseValue(line, "xoffset");
            c.yoffset = ParseValue(line, "yoffset");
            c.xadvance = ParseValue(line, "xadvance");

            chars[c.id] = c;
        }
    }
}

// Reads the integer after `key=` in an .fnt line (for example "x=10"). Returns 0 when missing.
int BitmapFont::ParseValue(const std::string& line, const std::string& key)
{
    std::string search = key + "=";
    size_t pos = line.find(search);
    if (pos == std::string::npos) return 0;

    size_t start = pos + search.length();
    size_t end = line.find(" ", start);
    if (end == std::string::npos) end = line.length();

    return std::stoi(line.substr(start, end - start));
}

void BitmapFont::Draw(const std::string& text, float startX, float startY, float scale, float r, float g, float b, float a)
{
    if (!sprite) return;

    auto dc = Graphics::Instance().GetDeviceContext();

    float cursorX = startX;
    float cursorY = startY;

    // 38 px per line at scale 1; the .fnt "common lineHeight" is not read
    float lineHeight = 38.0f * scale;

    for (size_t offset = 0; offset < text.size(); )
    {
        const size_t nextOffset = NextUtf8Offset(text, offset);
        const int id = DecodeUtf8Codepoint(text, offset, nextOffset);
        offset = nextOffset;

        if (id == '\n') // newline
        {
            cursorX = startX;
            cursorY += lineHeight;
            continue;
        }

        // skip characters missing from the font
        if (chars.find(id) == chars.end()) continue;

        CharData& data = chars[id];

        float drawX = cursorX + (data.xoffset * scale);
        float drawY = cursorY + (data.yoffset * scale);

        // Draw the glyph's cell of the atlas (sx, sy, sw, sh)
        sprite->Render(
            dc,
            drawX, drawY, 0.0f,                 // dx, dy, dz (screen position)
            (float)data.width * scale,          // dw (screen width)
            (float)data.height * scale,         // dh (screen height)
            (float)data.x, (float)data.y,       // sx, sy (cell position in the texture)
            (float)data.width, (float)data.height, // sw, sh (cell size in the texture)
            0.0f,                               // Angle
            r, g, b, a                          // color
        );

        cursorX += data.xadvance * scale;
    }
}

void BitmapFont::Draw3D(const std::string& text, const Camera* camera,
    DirectX::XMFLOAT3 worldPos, float scale, DirectX::XMFLOAT3 rotation, DirectX::XMFLOAT4 color)
{
    if (!sprite) return;
    auto dc = Graphics::Instance().GetDeviceContext();

    // -------------------------------------------------------------
    // Render state: alpha-blended text
    // -------------------------------------------------------------
    auto rs = Graphics::Instance().GetRenderState();

    // 1. Alpha blending, so the black background disappears
    dc->OMSetBlendState(rs->GetBlendState(BlendState::Transparency), nullptr, 0xFFFFFFFF);

    // 2. Depth test on, depth write off, so the transparent quads
    // do not cut holes in objects behind them
    dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestOnly), 0);

    // -------------------------------------------------------------

    using namespace DirectX;

    // Rotation of the whole text
    XMMATRIX matRot = XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z);

    // Cursor in text space
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float lineHeight = 38.0f * scale;

    for (size_t offset = 0; offset < text.size(); )
    {
        const size_t nextOffset = NextUtf8Offset(text, offset);
        const int id = DecodeUtf8Codepoint(text, offset, nextOffset);
        offset = nextOffset;

        if (id == '\n')
        {
            cursorX = 0.0f; // Reset X
            cursorY -= lineHeight; // Y decreases downward in 3D
            continue;
        }

        if (chars.find(id) == chars.end()) continue;

        CharData& data = chars[id];

        // 1. Glyph size in world units
        float w = data.width * scale;
        float h = data.height * scale;

        // 2. Offset from the text origin to this glyph
        // Screen Y grows down and world Y grows up, so Y is flipped.
        float localX = cursorX + (data.xoffset * scale) + (w / 2.0f); // +w/2: the sprite pivot is its centre
        float localY = cursorY - (data.yoffset * scale) - (h / 2.0f);

        // 3. Text space to world space, with the text rotation
        XMFLOAT3 localPos = { localX, localY, 0.0f };
        XMVECTOR vLocal = XMLoadFloat3(&localPos);
        vLocal = XMVector3TransformCoord(vLocal, matRot);

        XMFLOAT3 finalPos;
        XMStoreFloat3(&finalPos, vLocal + XMLoadFloat3(&worldPos));

        // 4. Draw the glyph
        sprite->Render3D(dc, camera,
            finalPos.x, finalPos.y, finalPos.z,
            w, h,
            (float)data.x, (float)data.y,
            (float)data.width, (float)data.height,
            rotation.x, rotation.y, rotation.z, // the glyph turns with the text
            color.x, color.y, color.z, color.w
        );

        // 5. Advance the cursor
        cursorX += data.xadvance * scale;
    }
}

DirectX::XMFLOAT2 BitmapFont::MeasureText(const std::string& text, float scale)
{
    float width = 0.0f;
    float maxWidth = 0.0f;
    int lineCount = 1;
    float lineHeight = 38.0f * scale; // same 38 as Draw(); the .fnt "common lineHeight" is not read

    if (text.empty()) return { 0.0f, 0.0f };

    for (size_t offset = 0; offset < text.size(); )
    {
        const size_t nextOffset = NextUtf8Offset(text, offset);
        const int id = DecodeUtf8Codepoint(text, offset, nextOffset);
        offset = nextOffset;

        if (id == '\n')
        {
            maxWidth = (std::max)(maxWidth, width);
            width = 0.0f;
            ++lineCount;
            continue;
        }

        if (chars.find(id) != chars.end())
        {
            // xadvance: distance from this glyph to the next
            width += chars[id].xadvance * scale;
        }
    }
    maxWidth = (std::max)(maxWidth, width);

    return { maxWidth, lineHeight * static_cast<float>(lineCount) };
}
