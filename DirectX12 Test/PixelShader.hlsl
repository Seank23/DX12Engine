struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

Texture2D gTexture : register(t0); // Texture bound at register t0
SamplerState gSampler : register(s0); // Sampler bound at register s0


float4 main(PSInput input) : SV_TARGET
{
    //return float4(input.texCoord, 0.5f, 1.0f); // Output the interpolated vertex color
    return gTexture.Sample(gSampler, input.texCoord);
}
