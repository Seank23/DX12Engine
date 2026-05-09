Texture2D uiTexture : register(t0);
SamplerState uiSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float2 uv       : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    float4 sampled = uiTexture.Sample(uiSampler, input.uv);
    return sampled * input.color;
}
