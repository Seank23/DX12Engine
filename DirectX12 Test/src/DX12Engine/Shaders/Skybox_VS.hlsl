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

struct PSInput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

PSInput main(VSInput input)
{
    PSInput output;
    
    // Remove translation from view matrix to keep skybox stationary
    float4x4 viewNoTranslation = ViewMatrix;
    viewNoTranslation._41_42_43_44 = float4(0, 0, 0, 1);
    matrix vpNoTranslation = viewNoTranslation * ProjectionMatrix;
    vpNoTranslation._41_42_43_44 = float4(0, 0, 0, 1);

    // Transform position
    output.position = mul(vpNoTranslation, float4(input.position, 1.0));
    output.texCoord = input.position; // Directly use position as texture coordinates

    return output;
}