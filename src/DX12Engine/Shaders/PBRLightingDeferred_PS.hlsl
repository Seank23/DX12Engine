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

cbuffer ScreenBuffer : register(b0)
{
    float4 CameraPosition;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
};

cbuffer LightBuffer : register(b1)
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

TextureCube environmentMap : register(t0);
TextureCube irradianceMap : register(t1);
Texture2D albedoMap : register(t2);
Texture2D worldNormalMap : register(t3);
Texture2D objectNormalMap : register(t4);
Texture2D materialMap : register(t5);
Texture2D positionMap : register(t6);
Texture2D emissiveMap : register(t7);
Texture2D depthMap : register(t8);
Texture2DArray shadowMaps : register(t9);
TextureCube shadowCubeMap : register(t10);
SamplerState samp : register(s0);
SamplerComparisonState shadowSampler : register(s1);

float3 sRGBToLinear(float3 color)
{
    return pow(max(color, 0.0), 2.2);
}

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

    return (D * G * F) / max(0.001, 4.0 * NdotV * NdotL);
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
    float clearcoatSpec = ClearcoatSpecLobe(NdotV, NdotL, NdotH, HdotV, clearcoatRoughness);
    color += clearcoat * clearcoatSpec;
    float3 reflectionVector = reflect(-V, N);
    float3 clearcoatEnv = sRGBToLinear(environmentMap.SampleLevel(samp, reflectionVector, clearcoatRoughness * 12.0).rgb);
    color += clearcoat * FcV * clearcoatEnv;
    
    return color;
}

float ShadowPCF(int lightIndex, float4 lightSpacePos, float softRadius)
{
    float3 texSize;
    shadowMaps.GetDimensions(texSize.x, texSize.y, texSize.z);
    float texelSize = 1.0 / texSize.x;
    
    float depth = lightSpacePos.z / lightSpacePos.w;
    float2 shadowUV = (lightSpacePos.xy / lightSpacePos.w) * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;
    
    if (shadowUV.x < 0.0 || shadowUV.x > 1.0 || shadowUV.y < 0.0 || shadowUV.y > 1.0 || depth < 0.0 || depth > 1.0)
        return 1.0;

    // 16-tap Poisson disk � irregular spacing breaks up the banding
    // that a regular grid produces at the shadow penumbra edge.
    static const float2 poissonDisk[16] =
    {
        float2(-0.94201624,  -0.39906216),
        float2( 0.94558609,  -0.76890725),
        float2(-0.09418410,  -0.92938870),
        float2( 0.34495938,   0.29387760),
        float2(-0.91588581,   0.45771432),
        float2(-0.81544232,  -0.87912464),
        float2(-0.38277543,   0.27676845),
        float2( 0.97484398,   0.75648379),
        float2( 0.44323325,  -0.97511554),
        float2( 0.53742981,  -0.47373420),
        float2(-0.26496911,  -0.41893023),
        float2( 0.79197514,   0.19090188),
        float2(-0.24188840,   0.99706507),
        float2(-0.81409955,   0.91437590),
        float2( 0.19984126,   0.78641367),
        float2( 0.14383161,  -0.14100790)
    };

    float radius = texelSize * softRadius;
    float shadow = 0.0;
    [unroll]
    for (int k = 0; k < 16; k++)
        shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV + poissonDisk[k] * radius, lightIndex), depth);
    return shadow / 16.0;
}

float PointLightShadowPCF(float3 worldPos, float3 lightPos, float softRadius, float3 normal, float farPlane)
{
    float3 texSize;
    shadowCubeMap.GetDimensions(0, texSize.x, texSize.y, texSize.z);
    float texelSize = 1.0 / texSize.x;
    float radius = texelSize * softRadius;
    
    float3 lightToFrag = worldPos - lightPos;
    float currentDepth = length(lightToFrag) / farPlane;
    float shadowBias = 0.005 / farPlane;
    float shadowFactor = 0.0;
    
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float3 sampleDir = normalize(lightToFrag) + float3(x, y, 0) * radius;
            float closestDepth = shadowCubeMap.Sample(samp, sampleDir).x;
            shadowFactor += (currentDepth - shadowBias > closestDepth) ? 0.0 : 1.0;
        }
    }
    shadowFactor /= 9.0;
    return shadowFactor;
}

float3 GetViewRay(float2 uv)
{
    float2 ndc = uv * 2.0f - 1.0f;
    float4 clipPos = float4(ndc.x, -ndc.y, 1.0f, 1.0f);
    float4 viewPos = mul(InvProjectionMatrix, clipPos);
    viewPos /= viewPos.w;
    return normalize(viewPos.xyz);
}

float4 main(PSInput input) : SV_TARGET
{
    float3 albedo = albedoMap.Sample(samp, input.texCoord).rgb;
    float3 worldNormal = normalize(worldNormalMap.Sample(samp, input.texCoord).rgb);
    float3 objectNormal = objectNormalMap.Sample(samp, input.texCoord).rgb;
    float4 material = materialMap.Sample(samp, input.texCoord);
    float roughness = saturate(material.r);
    float metallic = saturate(material.g);
    float clearcoat = saturate(material.b);
    float clearcoatRoughness = saturate(material.a);
    float3 emissive = emissiveMap.Sample(samp, input.texCoord).rgb;
    float ao = saturate(emissiveMap.Sample(samp, input.texCoord).a);
    float depth = depthMap.Sample(samp, input.texCoord).r;
    float3 worldPos = positionMap.Sample(samp, input.texCoord).rgb;
    
    float3 V = normalize(CameraPosition.xyz - worldPos);
    
    float3 finalColor = float3(0, 0, 0);
    float aoFactor = 0.02;
    float ambientShadow = 1.0;
    
    if (depth >= 0.999f)
    {
        float3 viewRay = GetViewRay(input.texCoord);
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
    float3 prefilteredEnv = sRGBToLinear(environmentMap.SampleLevel(samp, reflectionVector, perceptualRoughness * 12.0).rgb);
    float specularWeight = (1.0 - roughness) * (1.0 - roughness);
    float specOcc = SpecularOcclusion(NdotV, ao, roughness);
    float3 diffuseAmbient = (kD * diffuseIBL) * ao;
    float3 specularAmbient = prefilteredEnv * kS * specularWeight * specOcc;
    
    for (int i = 0; i < LightCount; i++)
    {
        float3 offsetPos = worldPos + (objectNormal * 0.04) + (worldNormal * aoFactor);
        float4 lightSpacePosition = mul(Lights[i].ViewProjMatrix, float4(offsetPos, 1.0));
        float3 lightDir = normalize(Lights[i].Position - worldPos);
        float shadowFactor = 1.0;
        float3 lightContribution = float3(0, 0, 0);
        
        if (Lights[i].Type == 0) // Directional Light
        {
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, normalize(-Lights[i].Direction), Lights[i]);
            shadowFactor = ShadowPCF(i, lightSpacePosition, 10.0);
            ambientShadow = min(ambientShadow, shadowFactor);
        }
        else if (Lights[i].Type == 1) // Point Light
        {
            float dist = length(Lights[i].Position - worldPos);
            float distOverRange = dist / Lights[i].Range;
            float window = saturate(1.0 - distOverRange * distOverRange * distOverRange * distOverRange);
            float attenuation = (window * window) / (dist * dist + 1.0);
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, Lights[i]) * attenuation;
            shadowFactor = PointLightShadowPCF(worldPos, Lights[i].Position, 3.0, worldNormal, Lights[i].Padding.x);
        }
        else if (Lights[i].Type == 2) // Spot Light
        {
            float theta = dot(lightDir, normalize(-Lights[i].Direction));
            float epsilon = cos(Lights[i].SpotAngle) - cos(Lights[i].SpotAngle) * 0.9;
            float intensity = saturate((theta - cos(Lights[i].SpotAngle * 0.9)) / epsilon);
            float dist = length(Lights[i].Position - worldPos);
            float attenuation = saturate(1.0 - (dist * dist) / (Lights[i].Range * Lights[i].Range));
            lightContribution = PBRLighting(albedo, metallic, roughness, clearcoat, clearcoatRoughness, worldNormal, V, lightDir, Lights[i]) * intensity * attenuation;
            shadowFactor = ShadowPCF(i, lightSpacePosition, 2.0);
            ambientShadow = min(ambientShadow, shadowFactor);
        }
        finalColor += lightContribution * shadowFactor;
    }
    // Apply the shadow-to-ambient gradient once on the accumulated result so the
    // full 0..1 PCF range maps smoothly to the min/max ambient levels.
    float smoothShadow = smoothstep(0.0, 1.0, ambientShadow);
    float remappedDiffuseShadow = lerp(0.2, 1.0, smoothShadow);
    float remappedSpecularShadow = lerp(0.05, 1.0, smoothShadow);
    finalColor += diffuseAmbient * remappedDiffuseShadow;
    finalColor += specularAmbient * remappedSpecularShadow;
    finalColor += emissive;
    return float4(finalColor, 1.0f);
}
