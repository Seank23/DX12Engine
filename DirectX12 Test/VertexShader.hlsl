// VertexShader.hlsl
struct VSInput
{
    float3 position : POSITION; // Position of the vertex
    float4 color : COLOR; // Color of the vertex
};

struct PSInput
{
    float4 position : SV_POSITION; // Transformed position
    float4 color : COLOR; // Passed color
};

PSInput main(VSInput input)
{
    PSInput output;
    // Directly output position to clip space (for a simple 2D triangle)
    output.position = float4(input.position, 1.0f);
    output.color = input.color; // Pass color to the pixel shader
    return output;
}