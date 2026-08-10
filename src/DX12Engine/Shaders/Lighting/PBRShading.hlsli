#ifndef PBR_SHADING_HLSLI
#define PBR_SHADING_HLSLI

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
	int ShadowMapIndex;
};

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float NormalDistribution(float NdotH, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    return alpha2 / (3.14159 * pow((NdotH * NdotH) * (alpha2 - 1.0) + 1.0, 2.0));
}

float GeometrySchlickGGX(float NdotV, float NdotL, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    return (NdotV / (NdotV * (1.0 - k) + k)) * (NdotL / (NdotL * (1.0 - k) + k));
}

float SpecularOcclusion(float NdotV, float ao, float roughness)
{
    // Prevent polished metals from glowing in shadow when only env lighting is present.
    float exponent = exp2(-16.0 * roughness - 1.0);
    return saturate(pow(saturate(NdotV + ao), exponent) - 1.0 + ao);
}

float ClearcoatSpecLobe(float NdotV, float NdotL, float NdotH, float HdotV, float clearcoatRoughness)
{
    float r = max(0.02, clearcoatRoughness * clearcoatRoughness);
    float D = NormalDistribution(NdotH, r);
    float G = GeometrySchlickGGX(NdotV, NdotL, r);

    // glTF clearcoat uses fixed dielectric F0 ~= 0.04
    float F = FresnelSchlick(HdotV, float3(0.04, 0.04, 0.04)).r;

    return (D * G * F) / max(0.01, 4.0 * NdotV * NdotL);
}

float3 PBRLighting(float3 albedo, float metallic, float roughness, float clearcoat, float clearcoatRoughness, float3 N, float3 V, float3 L, Light light)
{
    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);
    float3 F = FresnelSchlick(HdotV, F0);
    float D = NormalDistribution(NdotH, roughness);
    float G = GeometrySchlickGGX(NdotV, NdotL, roughness);
    
    float3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.001;
    float3 specular = numerator / denominator;
    float3 radiance = light.Color * NdotL * light.Intensity;
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = (kD * albedo) / 3.14159;
    float3 color = (diffuse + specular) * radiance;
    
    float FcV = FresnelSchlick(NdotV, float3(0.04, 0.04, 0.04)).r;
    float baseAtten = 1.0 - clearcoat * FcV;
    color *= baseAtten;
    float clearcoatSpec = (NdotL > 0.0) ? ClearcoatSpecLobe(NdotV, NdotL, NdotH, HdotV, clearcoatRoughness) : 0.0;
    color += clearcoat * clearcoatSpec * NdotL;
    
    return color;
}

#endif // PBR_SHADING_HLSLI
