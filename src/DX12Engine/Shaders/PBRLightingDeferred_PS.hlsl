#define MAX_LIGHTS 4
#define MAX_CSM_CASCADES 8

#include "Common/ColorUtils.hlsli"
#include "Lighting/PBRShading.hlsli"
#include "Common/ScreenData.hlsli"

cbuffer LightBuffer : register(b1)
{
    int LightCount;
    float3 Padding;
    Light Lights[MAX_LIGHTS];
};

cbuffer CascadedShadowBuffer : register(b2)
{
    float4x4 CascadeViewProj[MAX_CSM_CASCADES];
    float CascadeSplits[MAX_CSM_CASCADES];
    float CascadeTexelSize[MAX_CSM_CASCADES];
    float4 Params0; // x=count, y=maxDist, z=blend, w=unused
    float4 BiasParams; // x=const, y=slope, z=normal, w=unused
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

TextureCube environmentMap : register(t0);
TextureCube irradianceMap : register(t1);
Texture2D albedoMap : register(t2);
Texture2D normalsMap : register(t3);
Texture2D materialMap : register(t4);
Texture2D emissiveMap : register(t5);
Texture2D depthMap : register(t6);
Texture2DArray shadowMaps : register(t7);
TextureCubeArray shadowCubeMaps : register(t8);
Texture2DArray cascadedShadowMaps : register(t9);

SamplerState samp : register(s0);
SamplerComparisonState shadowSampler : register(s1);

#include "Common/GBufferUtils.hlsli"
#include "Shadows/ShadowSampling.hlsli"
#include "Shadows/CascadedShadowSampling.hlsli"

float4 main(PSInput input) : SV_TARGET
{
    float3 albedo = albedoMap.Sample(samp, input.texCoord).rgb;
    float4 packedNormals = LoadMap(input.texCoord, normalsMap);
    float3 worldNormal = UnpackNormal(packedNormals.xy);
    float3 objectNormal = UnpackNormal(packedNormals.zw);
    float4 material = materialMap.Sample(samp, input.texCoord);
    float roughness = saturate(material.r);
    float metallic = saturate(material.g);
    float clearcoat = saturate(material.b);
    float clearcoatRoughness = saturate(material.a);
    float3 emissive = emissiveMap.Sample(samp, input.texCoord).rgb;
    float ao = saturate(emissiveMap.Sample(samp, input.texCoord).a);
    float depth = depthMap.Sample(samp, input.texCoord).r;
    float3 worldPos = ReconstructWorldPos(input.texCoord, depth);
    
    float3 V = normalize(CameraPosition.xyz - worldPos);
    
    float3 finalColor = float3(0, 0, 0);
    float aoFactor = 0.02;
    float ambientShadow = 1.0;
    
    if (depth <= 0.001f) // Reverse-Z: the far plane / sky sits at depth 0.0
    {
        float3 viewRay = normalize(ReconstructViewPos(input.texCoord, depth));
        float3 worldDir = mul((float3x3)InvViewMatrix, viewRay);
        return float4(sRGBToLinear(environmentMap.Sample(samp, worldDir).rgb), 0.0);
    }
    
    float NdotV = saturate(dot(worldNormal, V));
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 kS = FresnelSchlickRoughness(NdotV, F0, roughness);
    float3 kD = (1.0 - kS) * (1.0 - metallic);
    float3 diffuseIBL = albedo * sRGBToLinear(irradianceMap.Sample(samp, worldNormal).rgb);
    float3 reflectionVector = reflect(-V, worldNormal);
    float perceptualRoughness = roughness;
    float baseGloss = 1.0 - roughness;
    float clearcoatGloss = clearcoat * (1.0 - clearcoatRoughness);
    float3 prefilteredEnv = sRGBToLinear(environmentMap.SampleLevel(samp, reflectionVector, perceptualRoughness * 12.0).rgb);
    float specularWeight = baseGloss * baseGloss;
    float specOcc = SpecularOcclusion(NdotV, ao, roughness);
    float clearcoatF = FresnelSchlick(NdotV, float3(0.04, 0.04, 0.04)).r;
    float baseAmbientAtten = 1.0 - clearcoat * clearcoatF;
    float3 diffuseAmbient = (kD * diffuseIBL) * ao;
    float3 specularAmbient = prefilteredEnv * kS * specularWeight * specOcc * baseAmbientAtten;
    float clearcoatOcc = SpecularOcclusion(NdotV, ao, clearcoatRoughness);
    float3 clearcoatEnv = sRGBToLinear(environmentMap.SampleLevel(samp, reflectionVector, clearcoatRoughness * 12.0).rgb);
    float3 clearcoatAmbient = clearcoatEnv * (clearcoat * clearcoatF * clearcoatOcc);
    
    for (int i = 0; i < LightCount; i++)
    {
        float3 offsetPos = worldPos + (objectNormal * 0.04) + (worldNormal * aoFactor);
        float4 lightSpacePosition = mul(Lights[i].ViewProjMatrix, float4(offsetPos, 1.0));
        float3 lightDir = normalize(Lights[i].Position - worldPos);
        float shadowFactor = 1.0;
        float3 lightContribution = float3(0, 0, 0);
        
        if (Lights[i].Type == 0) // Directional Light
        {
            float3 directionalL = normalize(-Lights[i].Direction);
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, directionalL, Lights[i]);
            shadowFactor = CascadedDirectionalShadow(worldPos, worldNormal, directionalL);
            ambientShadow = min(ambientShadow, shadowFactor);
        }
        else if (Lights[i].Type == 1) // Point Light
        {
            float dist = length(Lights[i].Position - worldPos);
            float distOverRange = dist / Lights[i].Range;
            float window = saturate(1.0 - distOverRange * distOverRange * distOverRange * distOverRange);
            float attenuation = (window * window) / (dist * dist + 1.0);
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, Lights[i]) * attenuation;
            shadowFactor = PointLightShadowPCF(worldPos, Lights[i].Position, 3.0, worldNormal, Lights[i].Padding.x, Lights[i].ShadowMapIndex);
        }
        else if (Lights[i].Type == 2) // Spot Light
        {
            float theta = dot(lightDir, normalize(-Lights[i].Direction));
            float epsilon = cos(Lights[i].SpotAngle) - cos(Lights[i].SpotAngle) * 0.9;
            float intensity = saturate((theta - cos(Lights[i].SpotAngle * 0.9)) / epsilon);
            float dist = length(Lights[i].Position - worldPos);
            float attenuation = saturate(1.0 - (dist * dist) / (Lights[i].Range * Lights[i].Range));
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, Lights[i]) * intensity * attenuation;
            shadowFactor = ShadowPCF(Lights[i].ShadowMapIndex, lightSpacePosition, 2.0);
            ambientShadow = min(ambientShadow, shadowFactor);
        }
        finalColor += lightContribution * shadowFactor;
    }

    // Apply the shadow-to-ambient gradient once on the accumulated result so the
    // full 0..1 PCF range maps smoothly to the min/max ambient levels.
    float smoothShadow = smoothstep(0.0, 1.0, ambientShadow);
    float remappedDiffuseShadow = lerp(0.2, 1.0, smoothShadow);
    float glossyShadowFloor = lerp(0.06, 0.30, saturate(max(baseGloss * 0.75, clearcoatGloss)));
    float remappedSpecularShadow = lerp(glossyShadowFloor, 1.0, smoothShadow);
    finalColor += diffuseAmbient * remappedDiffuseShadow;
    finalColor += (specularAmbient + clearcoatAmbient) * remappedSpecularShadow;
    finalColor += emissive;
    return float4(finalColor, 1.0f);
}
