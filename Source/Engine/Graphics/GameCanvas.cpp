#include "GameCanvas.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
#include <windows.h>

#include "Engine/Common/Constants.h"

namespace
{
    const char* const k_blitVertexShaderPath{ "Data/Shader/GameCanvasBlitVS.cso" };
    const char* const k_blitPixelShaderPath{ "Data/Shader/GameCanvasBlitPS.cso" };

    // Writes "[GameCanvas] <call> failed (hr=0x........)" to the debugger output.
    void LogFailure(const char* call, HRESULT hr)
    {
        std::array<char, 160> line{};
        std::snprintf(line.data(), line.size(), "[GameCanvas] %s failed (hr=0x%08lX)\n",
            call, static_cast<unsigned long>(hr));
        OutputDebugStringA(line.data());
    }

    // Reads a whole file into `bytes`. Returns false if it is missing or empty.
    bool ReadBinaryFile(const char* path, std::vector<char>& bytes)
    {
        std::ifstream file{ path, std::ios::binary };
        if (!file)
        {
            return false;
        }
        bytes.assign(std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{});
        return !bytes.empty();
    }
}

bool GameCanvas::Initialize(ID3D11Device* device)
{
    if (!device)
    {
        return false;
    }
    return CreateTargets(device) && CreateBlitPipeline(device);
}

bool GameCanvas::CreateTargets(ID3D11Device* device)
{
    using Microsoft::WRL::ComPtr;

    // The textures are not members: the views keep them alive.
    D3D11_TEXTURE2D_DESC colorDesc{};
    colorDesc.Width = static_cast<UINT>(Beyond::Config::CANVAS_WIDTH);
    colorDesc.Height = static_cast<UINT>(Beyond::Config::CANVAS_HEIGHT);
    colorDesc.MipLevels = 1;
    colorDesc.ArraySize = 1;
    // Why UNORM 8-bit: same precision as the window back buffer the scenes drew to before.
    colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorDesc.SampleDesc.Count = 1;
    colorDesc.Usage = D3D11_USAGE_DEFAULT;
    colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> colorTexture;
    HRESULT hr{ device->CreateTexture2D(&colorDesc, nullptr, colorTexture.GetAddressOf()) };
    if (FAILED(hr)) { LogFailure("CreateTexture2D (color)", hr); return false; }

    hr = device->CreateRenderTargetView(colorTexture.Get(), nullptr, m_colorTargetView.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateRenderTargetView", hr); return false; }

    hr = device->CreateShaderResourceView(colorTexture.Get(), nullptr, m_colorResourceView.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateShaderResourceView", hr); return false; }

    D3D11_TEXTURE2D_DESC depthDesc{ colorDesc };
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // same as Beyond::Window
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ComPtr<ID3D11Texture2D> depthTexture;
    hr = device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateTexture2D (depth)", hr); return false; }

    hr = device->CreateDepthStencilView(depthTexture.Get(), nullptr, m_depthStencilView.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateDepthStencilView", hr); return false; }

    return true;
}

bool GameCanvas::CreateBlitPipeline(ID3D11Device* device)
{
    std::vector<char> shaderBytes{};

    if (!ReadBinaryFile(k_blitVertexShaderPath, shaderBytes))
    {
        OutputDebugStringA("[GameCanvas] Cannot read Data/Shader/GameCanvasBlitVS.cso\n");
        return false;
    }
    HRESULT hr{ device->CreateVertexShader(shaderBytes.data(), shaderBytes.size(), nullptr,
        m_blitVertexShader.GetAddressOf()) };
    if (FAILED(hr)) { LogFailure("CreateVertexShader", hr); return false; }

    if (!ReadBinaryFile(k_blitPixelShaderPath, shaderBytes))
    {
        OutputDebugStringA("[GameCanvas] Cannot read Data/Shader/GameCanvasBlitPS.cso\n");
        return false;
    }
    hr = device->CreatePixelShader(shaderBytes.data(), shaderBytes.size(), nullptr,
        m_blitPixelShader.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreatePixelShader", hr); return false; }

    // Why linear + clamp: the window is rarely an integer multiple of the canvas, and
    // clamp keeps the image edge from bleeding in the opposite edge.
    D3D11_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device->CreateSamplerState(&samplerDesc, m_blitSampler.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateSamplerState", hr); return false; }

    D3D11_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;
    hr = device->CreateRasterizerState(&rasterizerDesc, m_blitRasterizerState.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateRasterizerState", hr); return false; }

    // Depth and stencil tests off. The remaining fields are still filled in, because
    // 0 is not a valid value for the comparison and stencil-op enums.
    const D3D11_DEPTH_STENCILOP_DESC unusedStencilOp{
        D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_STENCIL_OP_KEEP, D3D11_COMPARISON_ALWAYS };
    D3D11_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    depthDesc.StencilEnable = FALSE;
    depthDesc.FrontFace = unusedStencilOp;
    depthDesc.BackFace = unusedStencilOp;
    hr = device->CreateDepthStencilState(&depthDesc, m_blitDepthState.GetAddressOf());
    if (FAILED(hr)) { LogFailure("CreateDepthStencilState", hr); return false; }

    return true;
}

void GameCanvas::Begin(ID3D11DeviceContext* context) const
{
    ID3D11RenderTargetView* const target{ m_colorTargetView.Get() };
    context->OMSetRenderTargets(1, &target, m_depthStencilView.Get());

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(Beyond::Config::CANVAS_WIDTH);
    viewport.Height = static_cast<float>(Beyond::Config::CANVAS_HEIGHT);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    const std::array<float, 4> clearColor{ 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(target, clearColor.data());
    context->ClearDepthStencilView(m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void GameCanvas::Blit(ID3D11DeviceContext* context, const Beyond::PixelRect& destination) const
{
    if (destination.width <= 0 || destination.height <= 0)
    {
        return; // minimized window: nothing to draw into
    }

    // Why saved: Sprite draws with whatever sampler is in slot 0 and never sets one itself,
    // so the blit sampler must not stay bound.
    Microsoft::WRL::ComPtr<ID3D11SamplerState> previousSampler;
    context->PSGetSamplers(0, 1, previousSampler.GetAddressOf());

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = static_cast<float>(destination.x);
    viewport.TopLeftY = static_cast<float>(destination.y);
    viewport.Width = static_cast<float>(destination.width);
    viewport.Height = static_cast<float>(destination.height);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF); // null = opaque
    context->OMSetDepthStencilState(m_blitDepthState.Get(), 0);
    context->RSSetState(m_blitRasterizerState.Get());

    // The vertex shader builds one viewport-covering triangle from SV_VertexID,
    // so there is no vertex buffer and no input layout.
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_blitVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_blitPixelShader.Get(), nullptr, 0);

    ID3D11ShaderResourceView* const canvasTexture{ m_colorResourceView.Get() };
    context->PSSetShaderResources(0, 1, &canvasTexture);
    ID3D11SamplerState* const blitSampler{ m_blitSampler.Get() };
    context->PSSetSamplers(0, 1, &blitSampler);

    context->Draw(3, 0);

    // Why unbound: the canvas cannot be a shader input and a render target at the
    // same time, and the next Begin() binds it as target again.
    ID3D11ShaderResourceView* const noTexture{ nullptr };
    context->PSSetShaderResources(0, 1, &noTexture);
    ID3D11SamplerState* const restoredSampler{ previousSampler.Get() };
    context->PSSetSamplers(0, 1, &restoredSampler);
}
