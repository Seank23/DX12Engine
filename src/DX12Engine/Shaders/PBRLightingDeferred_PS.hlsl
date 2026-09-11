#define MAX_CSM_CASCADES 8

#include "Common/ColorUtils.hlsli"
#include "Lighting/PBRShading.hlsli"
#include "Common/ScreenData.hlsli"
#include "Lighting/ClusteredLighting.hlsli"

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

void ShadeLight(Light light, float3 worldPos, float3 worldNormal, float3 objectNormal, float3 V,
                float3 albedo, float metallic, float roughness, float clearcoat, float clearcoatRoughness,
                float aoFactor, inout float3 finalColor, inout float ambientShadow)
{
    float3 offsetPos = worldPos + (objectNormal * 0.04) + (worldNormal * aoFactor);
    float4 lightSpacePosition = mul(light.ViewProjMatrix, float4(offsetPos, 1.0));
    float3 lightDir = normalize(light.Position - worldPos);
    float shadowFactor = 1.0;
    float3 lightContribution = float3(0, 0, 0);
    
    if (light.Type == 0) // Directional Light
    {
        float3 directionalL = normalize(-light.Direction);
        lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, directionalL, light);
        shadowFactor = CascadedDirectionalShadow(worldPos, worldNormal, directionalL);
        ambientShadow = min(ambientShadow, shadowFactor);
    }
    else if (light.Type == 1) // Point Light
    {
        float dist = length(light.Position - worldPos);
        float distOverRange = dist / light.Range;
        float window = saturate(1.0 - distOverRange * distOverRange * distOverRange * distOverRange);
        float attenuation = (window * window) / (dist * dist + 1.0);
        lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, light) * attenuation;
		if (attenuation > 0.0)
			shadowFactor = (light.ShadowMapIndex >= 0) ? PointLightShadowPCF(worldPos, light.Position, 3.0, worldNormal, light.Padding1.x, light.ShadowMapIndex) : 1.0;
    }
    else if (light.Type == 2) // Spot Light
    {
        float cosOuter = cos(light.SpotAngle);
        float cosInner = cos(light.SpotAngle * 0.9);
        float theta = dot(lightDir, normalize(-light.Direction));
        float intensity = saturate((theta - cosOuter) / max(cosInner - cosOuter, 1e-4));
        float dist = length(light.Position - worldPos);
        float attenuation = saturate(1.0 - (dist * dist) / (light.Range * light.Range));
        lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, light) * intensity * attenuation;
        if (intensity * attenuation > 0.0)
			shadowFactor = (light.ShadowMapIndex >= 0) ? ShadowPCF(light.ShadowMapIndex, lightSpacePosition, 2.0) : 1.0;
	}
    finalColor += lightContribution * shadowFactor;
}

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
    float3 viewPos = ReconstructViewPos(input.texCoord, depth);
    
    float3 V = normalize(CameraPosition.xyz - worldPos);
    
    float3 finalColor = float3(0, 0, 0);
    float aoFactor = 0.02;
    // Sky/IBL occlusion, so only the sun's cascade may drive it - a local light's
    // shadow map says nothing about how much environment reaches the surface.
    float ambientShadow = 1.0;
    
    if (depth <= 0.0f)
    {
        float3 viewRay = normalize(viewPos);
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
    
    for (int i = 0; i < DirectionalLightCount; i++)
        ShadeLight(Lights[i], worldPos, worldNormal, objectNormal, V, albedo, metallic, roughness, clearcoat, clearcoatRoughness, aoFactor, finalColor, ambientShadow);

	uint2 range = ClusterGrid[GetClusterIndex(input.position.xy, viewPos.z)];
	if (ShowClusterHeatmap)
		return float4(ClusterHeatmapColour(range.y), 1.0f);
	
	for (int j = 0; j < range.y; j++)
		ShadeLight(Lights[LightIndices[range.x + j]], worldPos, worldNormal, objectNormal, V, albedo, metallic, roughness, clearcoat, clearcoatRoughness, aoFactor, finalColor, ambientShadow);

    // Apply the shadow-to-ambient gradient once on the accumulated result so the
    // full 0..1 PCF range maps smoothly to the min/max ambient levels.
    float smoothShadow = smoothstep(0.0, 1.0, ambientShadow);
    float remappedDiffuseShadow = lerp(0.2, 1.0, smoothShadow);
    float glossyShadowFloor = lerp(0.06, 0.30, saturate(max(baseGloss * 0.75, clearcoatGloss)));
    float remappedSpecularShadow = lerp(glossyShadowFloor, 1.0, smoothShadow);
    finalColor += diffuseAmbient * remappedDiffuseShadow * AmbientIntensity;
    finalColor += (specularAmbient + clearcoatAmbient) * remappedSpecularShadow * AmbientIntensity;
    finalColor += emissive;
    return float4(finalColor, 1.0f);
}
