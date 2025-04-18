#define MAX_LIGHTS 4

struct Light
{
    int Type; // 0 = Directional, 1 = Point, 2 = Spot
    float3 Position;
    float Intensity;
    float3 Direction;
    float Range;
    float3 Color;
    float SpotAngle;
    float3 Padding;
    matrix ViewProjMatrix;
};

cbuffer LightBuffer : register(b0)
{
    int LightCount;
    float3 Padding;
    Light Lights[MAX_LIGHTS];
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D materialMap : register(t2);
Texture2D positionMap : register(t3);
Texture2D depthMap : register(t4);
TextureCube environmentMap : register(t5);
TextureCube irradianceMap : register(t6);
Texture2DArray shadowMaps : register(t7);
TextureCube shadowCubeMap : register(t8);
SamplerState samp : register(s0);
SamplerComparisonState shadowSampler : register(s1);

float4 main(PSInput input) : SV_TARGET
{
    float3 albedo = albedoMap.Sample(samp, input.texCoord).rgb;
    float metallic = materialMap.Sample(samp, input.texCoord).r;
    float roughness = materialMap.Sample(samp, input.texCoord).g;
    float ao = materialMap.Sample(samp, input.texCoord).b;
    float depth = depthMap.Sample(samp, input.texCoord).r;
    float4 worldPosSample = positionMap.Sample(samp, input.texCoord);
    float3 worldPos = worldPosSample.xyz / worldPosSample.w;
    
	return float4(worldPos, 1.0f);
}