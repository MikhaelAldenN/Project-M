#pragma once

#include <vector>
#include <wrl.h>
#include <d3d11.h>
#include <DirectXMath.h>

class WindowSwarmRenderer
{
public:
    // 36 bytes per window: 2000 windows upload about 72 KB per frame.
    struct InstanceData
    {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 size;
        DirectX::XMFLOAT4 color;
    };

    WindowSwarmRenderer(ID3D11Device* device);

    // Call once at the start of each frame.
    void Begin();

    // Queues one window on the CPU; nothing is drawn yet.
    void AddWindow(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color);

    // Draws every queued window in one instanced draw call.
    void Render(ID3D11DeviceContext* dc, const DirectX::XMFLOAT4X4& viewProj, ID3D11ShaderResourceView* textureSRV);

private:
    static const UINT MaxInstances = 2000; // AddWindow ignores windows beyond this count

    struct QuadVertex {
        DirectX::XMFLOAT3 position;
        DirectX::XMFLOAT2 texcoord;
    };

    struct CbScene {
        DirectX::XMFLOAT4X4 viewProjection;
    };

    std::vector<InstanceData> m_instances;

    Microsoft::WRL::ComPtr<ID3D11VertexShader>  m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>   m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>   m_inputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_vertexBuffer;   // static quad
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_instanceBuffer; // per-instance data, rewritten every frame
    Microsoft::WRL::ComPtr<ID3D11Buffer>        m_constantBuffer;
};