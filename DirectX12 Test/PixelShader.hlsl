struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

cbuffer MaterialData : register(b1)
{
    float4 baseColor;
    int hasTexture;
};

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);


float4 main(PSInput input) : SV_TARGET
{
    float4 color = baseColor;
    if (hasTexture)
    {
        color *= gTexture.Sample(gSampler, input.texCoord);
    }
    return color;
}
