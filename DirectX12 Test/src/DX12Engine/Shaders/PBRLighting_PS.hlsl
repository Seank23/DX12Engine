struct PSInput
{
    float4 position : SV_POSITION;
    float3 cameraPos : POSITION0;
    float3 worldPos : POSITION1;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD;
};

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
};

cbuffer LightBuffer : register(b0)
{
    int LightCount;
    float3 Padding;
    Light Lights[4];
};

// PBR Material Data
cbuffer MaterialData : register(b1)
{
    float3 Albedo;
    float Metallic;
    float Roughness;
    float AO;
    float3 Emissive;
};

// Sampled Textures
Texture2D albedoMap : register(t0);
Texture2D normalMap : register(t1);
Texture2D metallicMap : register(t2);
Texture2D roughnessMap : register(t3);
Texture2D aoMap : register(t4);
SamplerState samp : register(s0);

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

float3 PBRLighting(float3 albedo, float metallic, float roughness, float3 N, float3 V, float3 L, float3 lightColor)
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
    float3 specular = numerator / denominator;
    
    float3 radiance = lightColor * NdotL;
    float3 kD = (1.0 - F) * (1.0 - metallic);
    
    float3 color = kD * albedo / 3.14159 + specular;
    return color * radiance;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 V = normalize(input.cameraPos - input.worldPos);

    float3 albedo = albedoMap.Sample(samp, input.texCoord).rgb;
    float metallic = metallicMap.Sample(samp, input.texCoord).r;
    float roughness = roughnessMap.Sample(samp, input.texCoord).r;
    float3 N = normalMap.Sample(samp, input.texCoord).rgb;
    
    float3 finalColor = float3(0, 0, 0);
    
    for (int i = 0; i < LightCount; i++)
    {
        float3 L = normalize(Lights[i].Position - input.worldPos);
        float3 lightColor = Lights[i].Color;
        finalColor += PBRLighting(albedo, metallic, roughness, N, V, L, lightColor);
    }

    return float4(finalColor, 1.0);
}