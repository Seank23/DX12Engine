cbuffer ConstantBuffer : register(b1)
{
    float4x4 ModelMatrix;
    float4x4 WVPMatrix;
    float3 CameraPosition;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION0;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

PSInput main(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(ModelMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;
    output.position = mul(WVPMatrix, float4(input.position, 1.0f));
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    return output;
}
