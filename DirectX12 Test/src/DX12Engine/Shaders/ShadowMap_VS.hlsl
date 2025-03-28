cbuffer ShadowConstants : register(b0)
{
    matrix LightViewProj;
}

struct VSInput
{
    float3 pos : POSITION;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = mul(LightViewProj, float4(input.pos, 1.0f));
    return output;
}