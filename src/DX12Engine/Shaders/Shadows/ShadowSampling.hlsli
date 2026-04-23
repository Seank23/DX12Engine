#ifndef SHADOW_SAMPLING_HLSLI
#define SHADOW_SAMPLING_HLSLI

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

    // 16-tap Poisson disk - irregular spacing breaks up the banding
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

#endif // SHADOW_SAMPLING_HLSLI
