// GameCanvasBlitVS - one triangle that covers the whole viewport, built from the
// vertex index alone (drawn with Draw(3, 0), no vertex buffer).

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    VSOutput output;

    // id 0 -> uv (0,0), id 1 -> uv (2,0), id 2 -> uv (0,2).
    // The part of the triangle inside the viewport maps to uv 0..1.
    output.texcoord = float2((vertexId << 1) & 2, vertexId & 2);
    output.position = float4(output.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);

    return output;
}
