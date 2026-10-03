#pragma once

#include <d3d11.h>
#include <wrl/client.h>

#include "Engine/Common/FitRect.h"

// GameCanvas - fixed-size off-screen render target (Config::CANVAS_WIDTH x CANVAS_HEIGHT).
// Scenes draw into the canvas; the engine then scales it into the main window, so
// scene code never depends on the window size. Owned by Framework.
class GameCanvas
{
public:
    GameCanvas() = default;
    GameCanvas(const GameCanvas&) = delete;
    GameCanvas& operator=(const GameCanvas&) = delete;

    // Creates the render target, depth buffer and blit pipeline. Call once at load time.
    // Returns false (and logs the failing call) if any GPU resource could not be created.
    [[nodiscard]] bool Initialize(ID3D11Device* device);

    // Binds the canvas as render target with a full-canvas viewport and clears it to black.
    void Begin(ID3D11DeviceContext* context) const;

    // Draws the canvas into `destination` (pixels) of the currently bound render target.
    // The caller binds that target first, and resets the viewport afterwards if it draws more.
    // Leaves blend, depth, rasterizer, shader and input-assembler state changed.
    void Blit(ID3D11DeviceContext* context, const Beyond::PixelRect& destination) const;

private:
    [[nodiscard]] bool CreateTargets(ID3D11Device* device);
    [[nodiscard]] bool CreateBlitPipeline(ID3D11Device* device);

    // ----- Canvas -----
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   m_colorTargetView;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_colorResourceView;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   m_depthStencilView;

    // ----- Blit pipeline -----
    Microsoft::WRL::ComPtr<ID3D11VertexShader>       m_blitVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>        m_blitPixelShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>       m_blitSampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>    m_blitRasterizerState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState>  m_blitDepthState;
};
