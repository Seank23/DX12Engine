Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D aoMap : register(t4);
SamplerState samp : register(s0);

cbuffer MaterialData : register(b1)
{
    float3 Albedo;
    float Metallic;
    float Roughness;
    float AO;
    float3 Emissive;
    float BaseColorAlpha;
    float NormalScale;
    float OcclusionStrength;
    int AlphaMode;
    float AlphaCutoff;
    int HasAlbedoMap;
    int HasNormalMap;
    int HasMetallicMap;
    int HasRoughnessMap;
    int HasAOMap;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 tangent : TANGENT;
    float3 bitangent : BITANGENT;
};

struct PSOutput
{
    float4 albedo : SV_Target0;
    float4 worldNormal : SV_Target1;
    float4 objectNormal : SV_Target2;
    float4 material : SV_Target3; // roughness, metallic, ao
    float4 position : SV_Target4;
};

float3 sRGBToLinear(float3 color)
{
    return pow(max(color, 0.0), 2.2);
}

PSOutput main(PSInput input)
{
    PSOutput output;

    float3 baseColor = HasAlbedoMap ? sRGBToLinear((float3) albedoMap.Sample(samp, input.uv)) : Albedo;
    float  metallic  = HasMetallicMap  ? (float)metallicMap.Sample(samp, input.uv).b  : Metallic;
    float  roughness = HasRoughnessMap ? (float)roughnessMap.Sample(samp, input.uv).g : Roughness;
    float  ao        = HasAOMap        ? (float)aoMap.Sample(samp, input.uv).r        : AO;
    float  alpha     = HasAlbedoMap    ? (float)albedoMap.Sample(samp, input.uv).a * BaseColorAlpha : BaseColorAlpha;

    float4 outputAlbedo = float4(baseColor, alpha);
    if (AlphaMode == 1) // Mask
    {
        clip(alpha - AlphaCutoff);
    }
    else if (AlphaMode == 2) // Blend
    {
        outputAlbedo = float4(baseColor * BaseColorAlpha, alpha);
    }
    
    float3 worldNormal;
    if (HasNormalMap)
    {
        float3 textureNormal = normalMap.Sample(samp, input.uv).rgb * 2.0 - 1.0;
        float3x3 TBN = float3x3(normalize(input.tangent), normalize(input.bitangent), normalize(input.normal));
        worldNormal = normalize(mul(textureNormal, TBN));
    }
    else
    {
        worldNormal = normalize(input.normal);
    }
    
    ao = lerp(1.0, ao, OcclusionStrength);

    output.albedo       = outputAlbedo;
    output.worldNormal  = float4(worldNormal, 1.0);
    output.objectNormal = float4(input.normal, 1.0);
    output.material     = float4(roughness, metallic, ao, 1.0);
    output.position     = float4(input.worldPos, 1.0);
    return output;
}