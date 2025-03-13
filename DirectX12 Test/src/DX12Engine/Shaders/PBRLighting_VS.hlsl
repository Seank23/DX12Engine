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
    float3 tangent : TANGENT;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 cameraPos : POSITION0;
    float3 worldPos : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.cameraPos = CameraPosition;
    float4 worldPos = mul(ModelMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;
    output.position = mul(WVPMatrix, float4(input.position, 1.0f));
    output.normal = input.normal;
    float4 tangent = normalize(mul(ModelMatrix, float4(input.tangent, 1.0)));
    output.tangent = tangent.xyz;
    output.bitangent = cross(output.tangent, output.normal);
    output.texCoord = input.texCoord;
    return output;
}
