#define SCREEN_DATA_REGISTER b1
#include "Common/ScreenData.hlsli"

cbuffer MaterialData : register(b2)
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

Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D aoMap : register(t4);
Texture2D emissiveMap : register(t5);
TextureCube envMap : register(t6);
Texture2D opaqueScene : register(t7);
Texture2D depthMap : register(t8);

SamplerState sampWrap : register(s0);
SamplerState sampClamp : register(s1);

struct PSInput
{
    float4 positionCS : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normalWS : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 tangentWS : TEXCOORD3;
    float3 bitangentWS : TEXCOORD4;
    float3 viewDirWS : TEXCOORD5;
};

#include "Common/ColorUtils.hlsli"
#include "Lighting/PBRShading.hlsli"
#define CLUSTER_DATA_REGISTER b3
#include "Lighting/ClusteredLighting.hlsli"

// The transparent pass binds no shadow atlases, so every light here shades unshadowed.
void ShadeLight(Light light, float3 worldPos, float3 worldNormal, float3 V,
                float3 albedo, float metallic, float roughness, float clearcoat, float clearcoatRoughness,
                inout float3 finalColor)
{
	float3 lightDir = normalize(light.Position - worldPos);
	float3 lightContribution = float3(0, 0, 0);
    
	if (light.Type == 0) // Directional Light
	{
		float3 directionalL = normalize(-light.Direction);
		lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, directionalL, light);
	}
	else if (light.Type == 1) // Point Light
	{
		float dist = length(light.Position - worldPos);
		float distOverRange = dist / light.Range;
		float window = saturate(1.0 - distOverRange * distOverRange * distOverRange * distOverRange);
		float attenuation = (window * window) / (dist * dist + 1.0);
		lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, light) * attenuation;
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
	}
	finalColor += lightContribution;
}

float4 main(PSInput input, bool isFrontFace : SV_IsFrontFace) : SV_TARGET
{
    // Manual depth test against the G-buffer depth so transparent fragments
    // are correctly occluded by opaque geometry without needing a bound DSV.
    float2 screenUVdepth = input.positionCS.xy / ScreenSize;
    float opaqueDepth = depthMap.Sample(sampClamp, screenUVdepth).r;
    // SV_POSITION.z is already the window-space depth in [MinDepth, MaxDepth]
    // that the hardware writes to the depth buffer, so no w-divide is needed.
    float fragDepth = input.positionCS.z;
    if (fragDepth < opaqueDepth) // Reverse-Z: smaller depth == behind opaque
        discard;
	float viewDepth = mul(ViewMatrix, float4(input.worldPos, 1.0)).z;
    float4 baseSample = HasAlbedoMap ? albedoMap.Sample(sampWrap, input.uv) : float4(1, 1, 1, 1);
	float3 baseColorRaw = baseSample.rgb * Albedo;
    float3 baseColor = sRGBToLinear(baseColorRaw);

    float baseAlpha = saturate(baseSample.a * BaseColorAlpha);

    float metallic = Metallic;
    float roughness = Roughness;
    if (HasMetallicMap != 0 && HasRoughnessMap != 0)
    {
        float4 m = metallicMap.Sample(sampWrap, input.uv);
        float4 r = roughnessMap.Sample(sampWrap, input.uv);
        roughness *= HasRoughnessMap ? r.g : 1.0;
        metallic *= HasMetallicMap ? m.b : 1.0;
    }

    float ao = AO;
    if (HasAOMap != 0)
        ao *= aoMap.Sample(sampWrap, input.uv).r;

    float faceSign = isFrontFace ? 1.0 : -1.0;
    float3 N = normalize(input.normalWS) * faceSign;
    if (HasNormalMap != 0)
    {
        float3 nTS = normalMap.Sample(sampWrap, input.uv).xyz * 2.0 - 1.0;
        nTS.xy *= NormalScale;
        nTS = normalize(nTS);
        float3 T = normalize(input.tangentWS) * faceSign;
        float3 B = normalize(input.bitangentWS) * faceSign;
        float3x3 TBN = float3x3(T, B, N);
        N = normalize(mul(nTS, TBN));
    }

    float3 V = normalize(input.viewDirWS);
    float NdotV = saturate(dot(N, V));

    float clampedIOR = max(IOR, 1.001);
    float iorF0 = pow((clampedIOR - 1.0) / (clampedIOR + 1.0), 2.0);
    float3 F0 = lerp(float3(iorF0, iorF0, iorF0), baseColor, metallic);
    if (Transmission > 0.0)
        F0 = float3(iorF0, iorF0, iorF0);
    float3 F = FresnelSchlick(NdotV, F0);

    float3 R = reflect(-V, N);
    float perceptualRoughness = roughness * roughness;
    float3 reflected = envMap.SampleLevel(sampWrap, R, perceptualRoughness * 12.0).rgb * AmbientIntensity;

    // Cheap screen-space refraction from existing opaque composite
    float2 screenUV = screenUVdepth;

    float eta = isFrontFace ? (1.0 / clampedIOR) : clampedIOR;
    float3 refrDir = refract(-V, N, eta);
    float2 refrUV = screenUV + refrDir.xy * saturate(RefractionScale);
    refrUV = saturate(refrUV);

    float3 refracted = opaqueScene.Sample(sampClamp, refrUV).rgb;

    // Simple Beer-Lambert style tinting so transmissive glass darkens/tints
    // instead of staying unnaturally bright in shaded regions.
    float3 absorptionTint = lerp(float3(1.0, 1.0, 1.0), saturate(baseColor), saturate(Transmission));
    float pathLength = lerp(1.0, 3.0, 1.0 - NdotV);
    float3 transmittance = pow(max(absorptionTint, float3(0.001, 0.001, 0.001)), pathLength);
    refracted *= transmittance;

    // Fresnel gives the specular reflection weight for this viewing angle.
    // Roughness attenuates it so rough surfaces show more of the diffuse/refracted
    // colour. Transmission blends between the opaque Fresnel reflection and a
    // glass-like mix where refraction dominates at low Fresnel angles.
    float fresnelMax = max(F.r, max(F.g, F.b));
    float roughAttenuation = 1.0 - perceptualRoughness;
    float specularWeight = fresnelMax * roughAttenuation;
    specularWeight *= lerp(0.55, 1.0, 1.0 - NdotV);
    // For transmissive materials lerp toward the refracted result; for opaque
    // materials the diffuse base colour tints the non-specular portion.
    float3 diffuseOrRefracted = lerp(refracted * baseColor, refracted, Transmission);
    float3 glassColor = lerp(diffuseOrRefracted, reflected, specularWeight) * ao;

    float3 emissive = Emissive * EmissiveStrength;
    if (HasEmissiveMap != 0)
    {
        float3 e = sRGBToLinear(emissiveMap.Sample(sampWrap, input.uv).rgb);
        emissive *= e;
    }

	float3 shadedColor = float3(0, 0, 0);
	for (int i = 0; i < DirectionalLightCount; i++)
		ShadeLight(Lights[i], input.worldPos, N, V, baseColorRaw, metallic, roughness, Clearcoat, ClearcoatRoughness, shadedColor);

	uint2 range = ClusterGrid[GetClusterIndex(input.positionCS.xy, viewDepth)];

	for (int j = 0; j < range.y; j++)
		ShadeLight(Lights[LightIndices[range.x + j]], input.worldPos, N, V, baseColorRaw, metallic, roughness, Clearcoat, ClearcoatRoughness, shadedColor);

    float transmissionAlpha = lerp(0.04, 0.18, 1.0 - NdotV);
    float outAlpha = saturate(lerp(baseAlpha, transmissionAlpha, saturate(Transmission)));

    float3 outColor = glassColor + shadedColor + emissive;
    return float4(outColor, outAlpha);
}
