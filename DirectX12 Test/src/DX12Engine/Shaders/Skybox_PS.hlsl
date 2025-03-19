struct PSInput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

Texture2D skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

float3 SampleEquirectangular(float3 dir)
{
    float2 uv;
    uv.x = (atan2(dir.z, dir.x) / (2.0 * 3.14159265359)) + 0.5;
    uv.y = acos(dir.y) / 3.14159265359;
    return skyboxTexture.Sample(samplerState, uv).rgb;
}

float4 main(PSInput input) : SV_TARGET
{
    return float4(SampleEquirectangular(input.texCoord), 1.0);
}