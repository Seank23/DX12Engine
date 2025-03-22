cbuffer CameraBuffer : register(b1)
{
    float4x4 ModelMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 MVPMatrix;
    float3 CameraPosition;
};

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    output.position = mul(ProjectionMatrix, mul(ViewMatrix, float4(input.position, 0.0f)));
    output.position.z = output.position.w;

    // Preserve direction for cube map sampling (use input position directly)
    output.texCoord = input.position;

    return output;
}