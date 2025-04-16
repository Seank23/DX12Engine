cbuffer ConstantBuffer : register(b0)
{
    float4x4 ModelMatrix;
    float4x4 NormalMatrix;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 MVPMatrix;
    float3 CameraPosition;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    float4 worldPosition = mul(ModelMatrix, float4(input.position, 1.0f));
    output.position = mul(MVPMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPosition.xyz;
    output.normal = normalize(mul(NormalMatrix, float4(input.normal, 0.0f)).xyz);
    output.uv = input.texCoord;
    float4 tangent = normalize(mul(ModelMatrix, float4(input.tangent, 1.0)));
    output.tangent = tangent.xyz / tangent.w;
    output.bitangent = cross(output.tangent, output.normal);
    return output;
}