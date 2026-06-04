Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D aoMap : register(t4);
Texture2D emissiveMap : register(t5);
SamplerState samp : register(s0);

cbuffer MaterialData : register(b1)
{
    float3 Albedo;
    float BaseColorAlpha;
    float3 Emissive;
    float EmissiveStrength;
    
    float Metallic;
    float Roughness;
    float AO;
    float OcclusionStrength;
    float Transmission;
    float IOR;
    float Clearcoat;
    float ClearcoatRoughness;
    
    float NormalScale;
    float RefractionScale;
    int AlphaMode;
    float AlphaCutoff;
    
    int HasAlbedoMap;
    int HasNormalMap;
    int HasMetallicMap;
    int HasRoughnessMap;
    int HasAOMap;
    int HasEmissiveMap;
};

struct PSInput
{
    float4 currentPosition : SV_POSITION;
    float4 currentClip : TEXCOORD0;
    float4 prevClip : TEXCOORD1;
    float3 worldPos : TEXCOORD2;
    float3 normal : TEXCOORD3;
    float2 uv : TEXCOORD4;
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
    float4 emissive : SV_Target5;
    float2 velocity : SV_Target6;
};

#include "Common/ColorUtils.hlsli"

PSOutput main(PSInput input)
{
    PSOutput output;

    float3 baseColor = HasAlbedoMap ? sRGBToLinear((float3) albedoMap.Sample(samp, input.uv)) : Albedo;
    float  metallic  = HasMetallicMap  ? (float)metallicMap.Sample(samp, input.uv).b  : Metallic;
    float  roughness = HasRoughnessMap ? (float)roughnessMap.Sample(samp, input.uv).g : Roughness;
    float  ao        = HasAOMap        ? (float)aoMap.Sample(samp, input.uv).r        : AO;
    float  alpha     = HasAlbedoMap    ? (float)albedoMap.Sample(samp, input.uv).a * BaseColorAlpha : BaseColorAlpha;
    
    float3 emissiveColor = Emissive * EmissiveStrength;
    if (HasEmissiveMap)
        emissiveColor *= sRGBToLinear(emissiveMap.Sample(samp, input.uv).rgb);

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
    
    float currentW = max(abs(input.currentClip.w), 1e-6);
    float prevW = max(abs(input.prevClip.w), 1e-6);
    float2 currentNDC = input.currentClip.xy / currentW;
    float2 prevNDC = input.prevClip.xy / prevW;
    float2 velocity = (currentNDC - prevNDC) * 0.5;
    velocity.y = -velocity.y;

    output.albedo       = outputAlbedo;
    output.worldNormal  = float4(worldNormal, 1.0);
    output.objectNormal = float4(input.normal, 1.0);
    output.material     = float4(roughness, metallic, Clearcoat, ClearcoatRoughness);
    output.position     = float4(input.worldPos, 1.0);
    output.emissive     = float4(emissiveColor, ao);
    output.velocity     = velocity;
    return output;
}
