cbuffer ShadowConstants : register(b0)
{
    matrix LightMVPMatrix;
    matrix ModelMatrix;
    float3 LightPos;
    float FarPlane;
}

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Position = mul(LightMVPMatrix, float4(input.Position, 1.0));
    return output;
}