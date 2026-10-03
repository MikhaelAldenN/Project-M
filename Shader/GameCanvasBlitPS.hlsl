// GameCanvasBlitPS - copies the game canvas to the bound render target.

Texture2D canvasTexture : register(t0);
SamplerState canvasSampler : register(s0);

float4 main(float4 position : SV_POSITION, float2 texcoord : TEXCOORD0) : SV_TARGET
{
    // Alpha forced to 1: the main window is opaque, whatever the scene left in canvas alpha.
    return float4(canvasTexture.Sample(canvasSampler, texcoord).rgb, 1.0f);
}
