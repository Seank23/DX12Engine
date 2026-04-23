#ifndef CASCADED_SHADOW_SAMPLING_HLSLI
#define CASCADED_SHADOW_SAMPLING_HLSLI

float3 GetCascadeShadowCoord(int cascadeIndex, float3 worldPos, float3 worldNormal, float NdotL)
{
    float normalBias = BiasParams.z * CascadeTexelSize[cascadeIndex];
    float3 biasedWorldPos = worldPos + worldNormal * normalBias;

    float4 lightSpacePos = mul(CascadeViewProj[cascadeIndex], float4(biasedWorldPos, 1.0));
    float invW = rcp(max(lightSpacePos.w, 1e-5));

    float2 shadowUV = lightSpacePos.xy * invW * 0.5 + 0.5;
    shadowUV.y = 1.0 - shadowUV.y;

    float slopeBias = BiasParams.x * BiasParams.y * (1.0 - saturate(NdotL));
    float depthBias = BiasParams.x + slopeBias;
    float depth = lightSpacePos.z * invW - depthBias;

    return float3(shadowUV, depth);
}

float CascadedShadowPCF(int cascadeIndex, float3 shadowCoord, float softRadius)
{
    uint width;
    uint height;
    uint layers;
    cascadedShadowMaps.GetDimensions(width, height, layers);

    if (cascadeIndex < 0 || cascadeIndex >= (int)layers)
        return 1.0;

    if (shadowCoord.x < 0.0 || shadowCoord.x > 1.0 || shadowCoord.y < 0.0 || shadowCoord.y > 1.0 || shadowCoord.z < 0.0 || shadowCoord.z > 1.0)
        return 1.0;

    float2 texelSize = 1.0 / float2(max(width, 1u), max(height, 1u));

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

    float radius = max(softRadius, 0.5);
    float shadow = 0.0;
    [unroll]
    for (int k = 0; k < 16; k++)
    {
        float2 sampleUV = shadowCoord.xy + poissonDisk[k] * texelSize * radius;
        shadow += cascadedShadowMaps.SampleCmpLevelZero(shadowSampler, float3(sampleUV, cascadeIndex), shadowCoord.z);
    }

    return shadow / 16.0;
}

float ViewDepthFromWorldPos(float3 worldPos)
{
    float3 viewPos = mul(ViewMatrix, float4(worldPos, 1.0)).xyz;
    return abs(viewPos.z);
}

int SelectCascadeIndex(float viewDepth, int cascadeCount)
{
    int cascadeIndex = 0;
    [loop]
    for (int i = 0; i < MAX_CSM_CASCADES - 1; ++i)
    {
        if (i < cascadeCount - 1 && viewDepth > CascadeSplits[i])
            cascadeIndex = i + 1;
    }
    return min(cascadeIndex, cascadeCount - 1);
}

float CascadedDirectionalShadow(float3 worldPos, float3 worldNormal, float3 L)
{
    int cascadeCount = clamp((int)Params0.x, 1, MAX_CSM_CASCADES);
    if (Params0.x < 0.5)
        return 1.0;

    float viewDepth = ViewDepthFromWorldPos(worldPos);
    if (viewDepth <= 0.0 || viewDepth > Params0.y)
        return 1.0;

    int cascadeIndex = SelectCascadeIndex(viewDepth, cascadeCount);

    float NdotL = saturate(dot(worldNormal, L));
    float3 shadowCoord = GetCascadeShadowCoord(cascadeIndex, worldPos, worldNormal, NdotL);
    float shadow = CascadedShadowPCF(cascadeIndex, shadowCoord, 2.0 + cascadeIndex);

    if (cascadeIndex < cascadeCount - 1)
    {
        float prevSplitDist = (cascadeIndex > 0) ? CascadeSplits[cascadeIndex - 1] : 0.0;
        float currSplitDist = CascadeSplits[cascadeIndex];
        float cascadeSpan = max(currSplitDist - prevSplitDist, 0.001);
        float blendRange = max(0.001, Params0.z * cascadeSpan);
        float blendStart = currSplitDist - blendRange;
        float blendWeight = saturate((viewDepth - blendStart) / blendRange);
        if (blendWeight > 0.0)
        {
            int nextCascade = cascadeIndex + 1;
            float3 nextCoord = GetCascadeShadowCoord(nextCascade, worldPos, worldNormal, NdotL);
            float nextShadow = CascadedShadowPCF(nextCascade, nextCoord, 2.0 + nextCascade);
            shadow = lerp(shadow, nextShadow, blendWeight);
        }
    }

    return shadow;
}

#endif // CASCADED_SHADOW_SAMPLING_HLSLI
