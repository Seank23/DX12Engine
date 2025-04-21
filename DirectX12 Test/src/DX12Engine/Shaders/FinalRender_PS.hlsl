struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

Texture2D finalRenderMap : register(t0);
SamplerState samp : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float3 output = finalRenderMap.Sample(samp, input.texCoord).rgb;
    return float4(output, 1.0);
}