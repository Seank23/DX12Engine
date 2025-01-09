cbuffer ConstantBuffer : register(b0)
{
    float4x4 wvpMatrix;
};

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

PSInput main(VSInput input)
{
    PSInput output;
    output.position = mul(wvpMatrix, float4(input.position, 1.0f));
    output.color = input.color;
    output.normal = input.normal;
    output.texCoord = input.texCoord;
    return output;
}
