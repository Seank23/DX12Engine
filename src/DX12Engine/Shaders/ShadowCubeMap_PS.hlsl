cbuffer ShadowConstants : register(b0)
{
    matrix LightMVPMatrix;
    matrix ModelMatrix;
    float3 LightPos;
    float FarPlane;
}

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

float main(PSInput input) : SV_Depth
{
    float dist = length(input.WorldPos - LightPos);
    return dist / FarPlane;
}