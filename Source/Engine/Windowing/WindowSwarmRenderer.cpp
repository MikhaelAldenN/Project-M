#include "WindowSwarmRenderer.h"
#include "System/GpuResourceUtils.h"
#include "System/Misc.h"

WindowSwarmRenderer::WindowSwarmRenderer(ID3D11Device* device)
{
    // 1. Input layout: per-vertex data in slot 0, per-instance data in slot 1
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        // Slot 0: static quad vertices
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },

        // Slot 1: per-instance data. Input slot 1 with D3D11_INPUT_PER_INSTANCE_DATA
        // advances once per instance instead of once per vertex.
        { "INST_POS",  0, DXGI_FORMAT_R32G32B32_FLOAT,    1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_SIZE", 0, DXGI_FORMAT_R32G32_FLOAT,       1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        { "INST_COL",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
    };

    GpuResourceUtils::LoadVertexShader(device, "Data/Shader/SwarmVS.cso", layout, ARRAYSIZE(layout), m_inputLayout.GetAddressOf(), m_vertexShader.GetAddressOf());
    GpuResourceUtils::LoadPixelShader(device, "Data/Shader/SwarmPS.cso", m_pixelShader.GetAddressOf());
    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbScene), m_constantBuffer.GetAddressOf());

    // 2. Vertex buffer: unit quad centred on the origin
    QuadVertex quad[4] = {
        { {-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f} },
        { { 0.5f,  0.5f, 0.0f}, {1.0f, 0.0f} },
        { {-0.5f, -0.5f, 0.0f}, {0.0f, 1.0f} },
        { { 0.5f, -0.5f, 0.0f}, {1.0f, 1.0f} }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(QuadVertex) * 4;
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vbData = { quad, 0, 0 };
    device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.GetAddressOf());

    // 3. CREATE INSTANCE BUFFER
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(InstanceData) * MaxInstances;
    ibDesc.Usage = D3D11_USAGE_DYNAMIC;
    ibDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;  // bound as the second vertex buffer (slot 1)
    ibDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&ibDesc, nullptr, m_instanceBuffer.GetAddressOf());

    m_instances.reserve(MaxInstances);
}

void WindowSwarmRenderer::Begin()
{
    m_instances.clear();
}

void WindowSwarmRenderer::AddWindow(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color)
{
    if (m_instances.size() < MaxInstances)
    {
        m_instances.push_back({ pos, size, color });
    }
}

void WindowSwarmRenderer::Render(ID3D11DeviceContext* dc, const DirectX::XMFLOAT4X4& viewProj, ID3D11ShaderResourceView* textureSRV)
{
    if (m_instances.empty() || !textureSRV) return;

    // 1. Update the constant buffer (view-projection)
    CbScene cb = { viewProj };
    dc->UpdateSubresource(m_constantBuffer.Get(), 0, 0, &cb, 0, 0);

    // 2. Update Instance Buffer (Map -> memcpy -> Unmap)
    // WRITE_DISCARD returns fresh memory, so the CPU does not wait for the GPU to finish reading last frame's data.
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(dc->Map(m_instanceBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, m_instances.data(), m_instances.size() * sizeof(InstanceData));
        dc->Unmap(m_instanceBuffer.Get(), 0);
    }

    // 3. Bind the pipeline state
    dc->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    dc->IASetInputLayout(m_inputLayout.Get());
    dc->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    dc->PSSetShaderResources(0, 1, &textureSRV);

    // 4. Bind the quad and the instance buffer to slots 0 and 1
    ID3D11Buffer* buffers[2] = { m_vertexBuffer.Get(), m_instanceBuffer.Get() };
    UINT strides[2] = { sizeof(QuadVertex), sizeof(InstanceData) };
    UINT offsets[2] = { 0, 0 };

    dc->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    dc->IASetVertexBuffers(0, 2, buffers, strides, offsets);

    // 5. Draw the 4-vertex quad once per instance
    dc->DrawInstanced(4, static_cast<UINT>(m_instances.size()), 0, 0);
}