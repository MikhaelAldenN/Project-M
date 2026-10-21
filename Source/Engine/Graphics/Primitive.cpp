#include "Primitive.h"
#include "Camera.h"
#include <string>  // std::wstring
#include <cmath>
#include <cassert>
#include <vector>
#include <cstdio>

Primitive::Primitive(ID3D11Device* device)
{
    // 1. Dynamic vertex buffer
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * MAX_VERTICES;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, vertexBuffer.GetAddressOf());

    // 2. Load the compiled shaders
    auto LoadShader = [&](const std::wstring& filename, std::vector<char>& buffer) {
        FILE* fp = nullptr;
        _wfopen_s(&fp, filename.c_str(), L"rb");
        if (!fp) return false;
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        buffer.resize(size);
        fread(buffer.data(), 1, size, fp);
        fclose(fp);
        return true;
        };

    std::vector<char> vsBlob, psBlob;
    if (LoadShader(L"Data/Shader/primitive_vs.cso", vsBlob)) {
        device->CreateVertexShader(vsBlob.data(), vsBlob.size(), nullptr, vertexShader.GetAddressOf());

        // Input Layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        device->CreateInputLayout(layout, 2, vsBlob.data(), vsBlob.size(), inputLayout.GetAddressOf());
    }

    if (LoadShader(L"Data/Shader/primitive_ps.cso", psBlob)) {
        device->CreatePixelShader(psBlob.data(), psBlob.size(), nullptr, pixelShader.GetAddressOf());
    }

    // 3. Rasterizer State
    D3D11_RASTERIZER_DESC rsDesc = {};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE;
    rsDesc.FrontCounterClockwise = false;
    device->CreateRasterizerState(&rsDesc, rasterizerState.GetAddressOf());

    // 4. Depth Stencil (Disable Z-Buffer for 2D)
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = FALSE;
    device->CreateDepthStencilState(&dsDesc, depthStencilState.GetAddressOf());
}

Primitive::~Primitive() {}

// --- Public drawing API ---

void Primitive::Rect(float x, float y, float w, float h, float cx, float cy, float angle, float r, float g, float b, float a)
{
    Rect(DirectX::XMFLOAT2{ x, y }, DirectX::XMFLOAT2{ w, h }, DirectX::XMFLOAT2{ cx, cy }, angle, DirectX::XMFLOAT4{ r, g, b, a });
}

void Primitive::Rect(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT2& center, float angle, const DirectX::XMFLOAT4& color)
{
    DrawRectInternal(position, size, center, angle, color);
}

void Primitive::Line(float x1, float y1, float x2, float y2, float width, float r, float g, float b, float a)
{
    // A line is a thin rotated rect
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float angle = atan2f(dy, dx);

    // Pivot at the left middle (0, width/2), so it rotates about the start point
    Rect(x1, y1, len, width, 0, width * 0.5f, angle, r, g, b, a);
}

void Primitive::Circle(float x, float y, float radius, float r, float g, float b, float a, int segments)
{
    // Not implemented: draws nothing.
}

void Primitive::Triangle(float x1, float y1, float x2, float y2, float x3, float y3, float r, float g, float b, float a)
{
    // Use brace initialization for the color vector
    const DirectX::XMFLOAT4 color{ r, g, b, a };

    // Push exactly 3 vertices to form one triangle in the batch
    batchVertices.push_back({ {x1, y1, 0.0f}, color });
    batchVertices.push_back({ {x2, y2, 0.0f}, color });
    batchVertices.push_back({ {x3, y3, 0.0f}, color });
}

// --- INTERNAL LOGIC ---

void Primitive::DrawRectInternal(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT2& center, float angle, const DirectX::XMFLOAT4& color)
{
    float cosA = cosf(angle);
    float sinA = sinf(angle);

    // Corners relative to the pivot
    // TL
    float x1 = -center.x;           float y1 = -center.y;
    // TR
    float x2 = -center.x + size.x;  float y2 = -center.y;
    // BL
    float x3 = -center.x;           float y3 = -center.y + size.y;
    // BR
    float x4 = -center.x + size.x;  float y4 = -center.y + size.y;

    auto Transform = [&](float x, float y) -> Vertex {
        Vertex v;
        v.position = {
            pos.x + (x * cosA - y * sinA),
            pos.y + (x * sinA + y * cosA),
            0.0f
        };
        v.color = color;
        return v;
        };

    Vertex vTL = Transform(x1, y1);
    Vertex vTR = Transform(x2, y2);
    Vertex vBL = Transform(x3, y3);
    Vertex vBR = Transform(x4, y4);

    // PUSH 6 VERTEX (TRIANGLE LIST)
    // Triangle 1 (TL -> TR -> BL)
    batchVertices.push_back(vTL);
    batchVertices.push_back(vTR);
    batchVertices.push_back(vBL);

    // Triangle 2 (TR -> BR -> BL)
    batchVertices.push_back(vTR);
    batchVertices.push_back(vBR);
    batchVertices.push_back(vBL);
}

void Primitive::Render(ID3D11DeviceContext* context)
{
    if (batchVertices.empty()) return;

    UINT num = 1;
    D3D11_VIEWPORT vp;
    context->RSGetViewports(&num, &vp);
    float screenW = vp.Width;
    float screenH = vp.Height;

    D3D11_MAPPED_SUBRESOURCE msr;
    if (SUCCEEDED(context->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        Vertex* ptr = (Vertex*)msr.pData;
        for (const auto& v : batchVertices)
        {
            Vertex finalV = v;
            // Pixels to NDC
            finalV.position.x = (v.position.x / screenW) * 2.0f - 1.0f;
            finalV.position.y = -((v.position.y / screenH) * 2.0f - 1.0f);
            *ptr++ = finalV;
        }
        context->Unmap(vertexBuffer.Get(), 0);
    }

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetInputLayout(inputLayout.Get());

    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->PSSetShader(pixelShader.Get(), nullptr, 0);
    context->RSSetState(rasterizerState.Get());
    context->OMSetDepthStencilState(depthStencilState.Get(), 0);

    context->Draw((UINT)batchVertices.size(), 0);
    batchVertices.clear();
}

DirectX::XMFLOAT2 WorldToViewportPixels(ID3D11DeviceContext* context, const Camera& camera,
    const DirectX::XMFLOAT3& worldPosition)
{
    UINT viewportCount{ 1 };
    D3D11_VIEWPORT viewport{};
    context->RSGetViewports(&viewportCount, &viewport);

    const DirectX::XMMATRIX view{ DirectX::XMLoadFloat4x4(&camera.GetView()) };
    const DirectX::XMMATRIX projection{ DirectX::XMLoadFloat4x4(&camera.GetProjection()) };

    // Why origin (0, 0): Primitive::Render maps pixels relative to the viewport's
    // own top-left corner, not the render target's.
    const DirectX::XMVECTOR projected{ DirectX::XMVector3Project(
        DirectX::XMLoadFloat3(&worldPosition),
        0.0f, 0.0f, viewport.Width, viewport.Height, 0.0f, 1.0f,
        projection, view, DirectX::XMMatrixIdentity()) };

    DirectX::XMFLOAT2 pixels{};
    DirectX::XMStoreFloat2(&pixels, projected);
    return pixels;
}
