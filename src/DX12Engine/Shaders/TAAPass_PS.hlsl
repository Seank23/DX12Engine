cbuffer ScreenBuffer : register(b0)
{
    float4 CameraPosition;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
    float2 Jitter;
    float2 PrevJitter;
};

cbuffer TemporalBuffer : register(b1)
{
    float4x4 PrevViewMatrix;
    float4x4 PrevProjectionMatrix;
    uint FrameIndex;
    float3 TemporalPadding;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color   : SV_TARGET0;
    float4 history : SV_TARGET1;
};

Texture2D sceneColorMap : register(t0);
Texture2D depthMap      : register(t1);
Texture2D velocityMap   : register(t2);
Texture2D historyMap    : register(t3);

SamplerState samp : register(s0);

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 SampleCurrent(float2 uv)
{
    return sceneColorMap.Sample(samp, uv).rgb;
}

float2 GetJitterDelta(float2 uv)
{
    // Jitter values are in pixel units in [-0.5, 0.5].
    float2 jitterDeltaPixels = PrevJitter - Jitter;
    float2 jitterDeltaUV = jitterDeltaPixels / ScreenSize;
    return jitterDeltaUV;
}

float2 ReprojectFromVelocity(float2 uv)
{
    float2 velocity = velocityMap.Sample(samp, uv).xy;
    return uv - velocity;
}

void NeighborhoodMinMax(float2 uv, out float3 cMin, out float3 cMax)
{
    float2 texel = 1.0 / ScreenSize;
    cMin = float3(1e9, 1e9, 1e9);
    cMax = float3(-1e9, -1e9, -1e9);

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 p = uv + float2((float)x, (float)y) * texel;
            float3 c = SampleCurrent(p);
            cMin = min(cMin, c);
            cMax = max(cMax, c);
        }
    }
}

float EdgeFactor(float2 uv)
{
    float2 texel = 1.0 / ScreenSize;

    float zc = depthMap.Sample(samp, uv).r;
    float zx = depthMap.Sample(samp, uv + float2(texel.x, 0.0)).r;
    float zy = depthMap.Sample(samp, uv + float2(0.0, texel.y)).r;

    float dz = abs(zx - zc) + abs(zy - zc);
    return saturate(dz * 64.0);
}

float3 Sharpen(float2 uv, float3 color)
{
    float2 texel = 1.0 / ScreenSize;
    float3 n = SampleCurrent(uv + float2(0.0, texel.y));
    float3 s = SampleCurrent(uv - float2(0.0, texel.y));
    float3 e = SampleCurrent(uv + float2(texel.x, 0.0));
    float3 w = SampleCurrent(uv - float2(texel.x, 0.0));
    float3 blur = (n + s + e + w) * 0.25;
    return max(color + (color - blur) * 0.05, 0.0);
}

PSOutput main(PSInput input)
{
    PSOutput output;
    float2 uv = input.texCoord;

    float3 current = SampleCurrent(uv);
    float2 prevUV = ReprojectFromVelocity(uv);

    // First frame (or invalid reprojection) starts fresh.
    bool outOfBounds = any(prevUV < 0.0) || any(prevUV > 1.0);
    if (FrameIndex == 0 || outOfBounds)
    {
        float3 start = Sharpen(uv, current);
        output.color = float4(start, 1.0);
        output.history = float4(start, 1.0);
        return output;
    }

    float2 velocity = velocityMap.Sample(samp, uv).xy;
    float velocityPixels = length(velocity * ScreenSize);
    float velocityFactor = saturate(velocityPixels * 0.2);

    float3 history = historyMap.Sample(samp, prevUV).rgb;

    // Clamp history into a local box around the current frame neighborhood.
    float3 cMin, cMax;
    NeighborhoodMinMax(uv, cMin, cMax);
    float3 center = current;
    float3 halfExtent = (cMax - cMin) * 0.5;
    // Tighten the clamp under motion to avoid chroma smearing/ghost tails.
    halfExtent *= lerp(1.0, 0.35, velocityFactor);
    history = clamp(history, center - halfExtent, center + halfExtent);

    // Reduce history trust near geometric edges and when history diverges heavily.
    float edge = EdgeFactor(uv);
    float lumaCurrent = Luma(current);
    float lumaHistory = Luma(history);
    float contrast = saturate(abs(lumaCurrent - lumaHistory) * 4.0);
    float colorDelta = saturate(length(current - history) * 1.5);
    float depthCurrent = depthMap.Sample(samp, uv).r;
    float depthAtPrevUV = depthMap.Sample(samp, prevUV).r;
    float disocclusion = saturate(abs(depthCurrent - depthAtPrevUV) * 250.0);

    // Base history weight is intentionally conservative to reduce visible ghosting.
    float historyWeight = 0.78;
    historyWeight *= lerp(1.0, 0.45, edge);
    historyWeight *= lerp(1.0, 0.25, velocityFactor);
    historyWeight *= lerp(1.0, 0.08, disocclusion);
    historyWeight *= (1.0 - 0.65 * contrast);
    historyWeight *= (1.0 - 0.55 * colorDelta);
    historyWeight = saturate(historyWeight);
    historyWeight = clamp(historyWeight, 0.03, 0.82);

    float3 resolved = lerp(current, history, historyWeight);
    // Avoid adding sharpness in strongly disoccluded regions where it amplifies halos.
    float sharpenAmount = 1.0 - saturate(disocclusion * 1.25);
    resolved = lerp(resolved, Sharpen(uv, resolved), sharpenAmount);

    output.color = float4(resolved, 1.0);
    output.history = float4(resolved, 1.0);
    return output;
}
