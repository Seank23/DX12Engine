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

cbuffer LightingPassBuffer : register(b1)
{
    float4 CameraPosition;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
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
Texture2D depthMap : register(t7);
Texture2DArray shadowMaps : register(t8);
TextureCube shadowCubeMap : register(t9);
SamplerState samp : register(s0);
SamplerComparisonState shadowSampler : register(s1);

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
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

float3 PBRLighting(float3 albedo, float metallic, float roughness, float ao, float3 N, float3 V, float3 L, Light light)
{
    float3 H = normalize(V + L);
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);
    
    float3 F0 = lerp(0.04, albedo, metallic);
    float3 F = FresnelSchlick(HdotV, F0);
    float D = NormalDistribution(NdotH, roughness);
    float G = GeometrySchlickGGX(NdotV, NdotL, roughness);
    
    float3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.001;
    float3 specularLight = numerator / denominator;
    
    float3 reflectionVector = reflect(-V, N);
    float3 specularEnv = environmentMap.SampleLevel(samp, reflectionVector, roughness * 12).rgb;
    
    float3 specular = specularLight * specularEnv;
    
    float3 radiance = light.Color * NdotL * light.Intensity;
    float3 kD = (1.0 - F) * (1.0 - metallic);
    
    float3 color = (kD * albedo * ao / 3.14159) + specular;
    return color * radiance;
}

float ShadowPCF(int lightIndex, float4 lightSpacePos, float softRadius)
{
    float shadow = 0.0f;
    float3 texSize;
    shadowMaps.GetDimensions(texSize.x, texSize.y, texSize.z);
    float texelSize = 1.0 / texSize.x;
    float radius = texelSize * softRadius;
    
    float depth = lightSpacePos.z / lightSpacePos.w;
    float2 shadowUV = (lightSpacePos.xy / lightSpacePos.w) * 0.5 + 0.5;
    
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float2 transformedUV = shadowUV + float2(x, y) * radius;
            shadow += shadowMaps.SampleCmpLevelZero(shadowSampler, float3(transformedUV, lightIndex), depth);
        }
    }
    shadow /= 9.0;
    
    float bias = 0.5;
    return shadow + bias;
}

float PointLightShadowPCF(float3 worldPos, float3 lightPos, float softRadius, float3 normal)
{
    float3 texSize;
    shadowCubeMap.GetDimensions(0, texSize.x, texSize.y, texSize.z);
    float texelSize = 1.0 / texSize.x;
    float radius = texelSize * softRadius;
    
    float3 lightToFrag = (worldPos - lightPos) + normal * 0.05;
    float lightDepth = length(lightToFrag) * 0.28;
    float shadowBias = 0.002;
    float shadowFactor = 0.0;
    
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            float shadow = 0.0;
            float3 transformedUV = normalize(lightToFrag) + float3(x, y, 0) * radius;
            shadow = shadowCubeMap.Sample(samp, transformedUV).x;
            if (shadow + shadowBias < lightDepth && shadow < 0.92)
                shadowFactor += shadow / min(lightDepth * 2.0, 3.0);
            else
                shadowFactor += 1.0;
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
    float3 albedo = albedoMap.Sample(samp, input.texCoord);
    float3 worldNormal = worldNormalMap.Sample(samp, input.texCoord);
    float3 objectNormal = objectNormalMap.Sample(samp, input.texCoord);
    float roughness = materialMap.Sample(samp, input.texCoord).r;
    float metallic = materialMap.Sample(samp, input.texCoord).g;
    float ao = materialMap.Sample(samp, input.texCoord).b;
    float depth = depthMap.Sample(samp, input.texCoord).r;
    float3 worldPos = positionMap.Sample(samp, input.texCoord);
    
    float3 V = normalize(CameraPosition.xyz - worldPos);
    
    float3 finalColor = float3(0, 0, 0);
    float shadowFactor = 1.0;
    float aoFactor = 0.02;
    
    if (depth >= 0.999f)
    {
        float3 viewRay = GetViewRay(input.texCoord);
        float3 worldDir = mul((float3x3)InvViewMatrix, viewRay);
        return float4(environmentMap.Sample(samp, worldDir).rgb, 1.0);
    }
    
    float3 indirectDiffuse = albedo * irradianceMap.Sample(samp, worldNormal).rgb;
    finalColor += indirectDiffuse;
    
    for (int i = 0; i < LightCount; i++)
    {
        float3 offsetPos = worldPos + (objectNormal * 0.04) + (worldNormal * aoFactor);
        float4 lightSpacePosition = mul(Lights[i].ViewProjMatrix, float4(offsetPos, 1.0));
        lightSpacePosition.y *= -1;
        float3 lightDir = normalize(Lights[i].Position - worldPos);
        
        if (Lights[i].Type == 0) // Directional Light
        {
            finalColor += PBRLighting(albedo, metallic, roughness, ao, worldNormal, V, normalize(-Lights[i].Direction), Lights[i]);
            shadowFactor *= ShadowPCF(i, lightSpacePosition, 2.0);
        }
        else if (Lights[i].Type == 1) // Point Light
        {
            float dist = length(Lights[i].Position - worldPos);
            float attenuation = saturate(1.0 - (dist * dist) / (Lights[i].Range * Lights[i].Range));
            finalColor += PBRLighting(albedo, metallic, roughness, ao, worldNormal, V, lightDir, Lights[i]) * attenuation;
            shadowFactor *= PointLightShadowPCF(worldPos, Lights[i].Position, 3.0, worldNormal);
        }
        else if (Lights[i].Type == 2) // Spot Light
        {
            float theta = dot(lightDir, normalize(-Lights[i].Direction));
            float epsilon = cos(Lights[i].SpotAngle) - cos(Lights[i].SpotAngle) * 0.9;
            float intensity = saturate((theta - cos(Lights[i].SpotAngle * 0.9)) / epsilon);
            float dist = length(Lights[i].Position - worldPos);
            float attenuation = saturate(1.0 - (dist * dist) / (Lights[i].Range * Lights[i].Range));
            finalColor += PBRLighting(albedo, metallic, roughness, ao, worldNormal, V, lightDir, Lights[i]) * intensity * attenuation;
            shadowFactor *= ShadowPCF(i, lightSpacePosition, 2.0);
        }
    }
    finalColor *= shadowFactor;
	return float4(finalColor, 1.0f);
}