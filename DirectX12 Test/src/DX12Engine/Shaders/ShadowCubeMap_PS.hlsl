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
};

float main(PSInput input) : SV_Depth
{
    return input.Position.z;
}