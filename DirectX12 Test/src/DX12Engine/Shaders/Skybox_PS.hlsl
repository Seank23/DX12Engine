struct PSInput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD;
};

Texture2D skyboxTexture : register(t0);
SamplerState samplerState : register(s0);

float2 SampleSphericalMap(float3 v)
{
    float phi = atan2(v.z, v.x); // Longitude
    float theta = acos(v.y); // Latitude

    float u = phi / (2.0f * 3.14159265359f) + 0.5f;
    float vTex = theta / 3.14159265359f;

    return float2(u, vTex);
}

float4 main(PSInput input) : SV_TARGET
{
    float2 uv = SampleSphericalMap(normalize(input.texCoord));
    return skyboxTexture.Sample(samplerState, uv);
}