#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <wrl/client.h>

class Primitive
{
public:
    Primitive(ID3D11Device* device);
    ~Primitive();

    // --- High-level drawing ---
    // Rectangle (floats)
    void Rect(float x, float y, float w, float h,
        float cx, float cy, float angle,
        float r, float g, float b, float a);

    // Rectangle (vectors)
    void Rect(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT2& center, float angle,
        const DirectX::XMFLOAT4& color);

    // Line
    void Line(float x1, float y1, float x2, float y2,
        float width, float r, float g, float b, float a);

    // Circle (not implemented: draws nothing)
    void Circle(float x, float y, float radius,
        float r, float g, float b, float a, int segments = 32);

    // Triangle
    void Triangle(float x1, float y1, float x2, float y2, float x3, float y3,
        float r, float g, float b, float a);

    // --- Rendering ---
    // Draws the queued primitives and clears the queue.
    void Render(ID3D11DeviceContext* context);

private:
    // Vertex layout of the primitive shader
    struct Vertex
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT4 color;
    };

    // Low-level function (Internal)
    void DrawRectInternal(const DirectX::XMFLOAT2& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT2& center, float angle, const DirectX::XMFLOAT4& color);

    // DirectX Resources
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthStencilState;

    // Queued vertices, drawn by Render
    std::vector<Vertex> m_batchVertices;
    const size_t m_maxVertices = 2048; // Buffer size
};

class Camera;

// Projects a world-space point through `camera` into pixel coordinates of the viewport
// currently bound on `context`. That is the pixel space Primitive draws in, so the
// result can be passed straight to Primitive::Rect / Line. Pass the camera of the
// window being rendered; the result is then correct for any window size or position.
DirectX::XMFLOAT2 WorldToViewportPixels(ID3D11DeviceContext* context, const Camera& camera,
    const DirectX::XMFLOAT3& worldPosition);