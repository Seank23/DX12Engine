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
    uint FrameIndex;
    uint EnableHistoryReset;
    float BaseBlend;
    float MinBlend;
    float MaxBlend;
    float VelocityRejection;
    float DepthRejection;
    float ClampGamma;
    float Sharpness;
    float DisocclusionDepthThreshold;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

struct PSOutput
{
    float4 color : SV_TARGET0;
    float4 history : SV_TARGET1;
};

Texture2D sceneColorMap : register(t0);
Texture2D depthMap : register(t1);
Texture2D velocityMap : register(t2);
Texture2D historyMap : register(t3);

SamplerState samp : register(s0);

float Luma(float3 c)
{
    return dot(c, float3(0.2126, 0.7152, 0.0722));
}

float3 SampleCurrent(float2 uv)
{
    return sceneColorMap.Sample(samp, uv).rgb;
}

float3 PrefilterCurrent(float2 uv)
{
    float2 texel = 1.0 / ScreenSize;
    float3 sum = 0.0;
    float weightSum = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 o = float2((float) x, (float) y);
            float w = (x == 0 && y == 0) ? 4.0 : ((x == 0 || y == 0) ? 2.0 : 1.0);
            sum += sceneColorMap.SampleLevel(samp, uv + o * texel, 0).rgb * w;
            weightSum += w;
        }
    }

    return sum / weightSum;
}

float CatmullRom(float x)
{
    x = abs(x);
    if (x <= 1.0)
        return 1.5 * x * x * x - 2.5 * x * x + 1.0;
    if (x < 2.0)
        return -0.5 * x * x * x + 2.5 * x * x - 4.0 * x + 2.0;
    return 0.0;
}

float3 SampleHistoryCatmullRom(float2 uv)
{
    float2 texel = 1.0 / ScreenSize;
    float2 samplePos = uv * ScreenSize - 0.5;
    float2 base = floor(samplePos);
    float2 f = samplePos - base;

    float3 accum = 0.0;
    float totalWeight = 0.0;

    [unroll]
    for (int y = -1; y <= 2; ++y)
    {
        [unroll]
        for (int x = -1; x <= 2; ++x)
        {
            float2 offset = float2((float) x, (float) y);
            float2 tapUV = (base + offset + 0.5) * texel;
            float w = CatmullRom(offset.x - f.x) * CatmullRom(offset.y - f.y);
            accum += historyMap.SampleLevel(samp, tapUV, 0).rgb * w;
            totalWeight += w;
        }
    }

    return (totalWeight > 0.0) ? (accum / totalWeight) : historyMap.SampleLevel(samp, uv, 0).rgb;
}

float2 JitterDeltaUV()
{
    // Jitter is stored in pixel units in [-0.5, 0.5].
    // Reprojection must account for frame-to-frame jitter phase shift.
    float2 jitterDeltaPixels = PrevJitter - Jitter;
    return jitterDeltaPixels / ScreenSize;
}

// Dilate velocity by picking the motion vector from the closest-depth
// neighbor in a 3x3 region.  This prevents thin geometry and edges from
// getting an incorrect (background) motion vector, which is a major
// source of edge shimmer.
float2 DilatedVelocity(float2 uv)
{
    float2 texel = 1.0 / ScreenSize;
    float bestDepth = 1.0;
    float2 bestUV = uv;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 tapUV = uv + float2((float) x, (float) y) * texel;
            float d = depthMap.SampleLevel(samp, tapUV, 0).r;
            if (d < bestDepth)
            {
                bestDepth = d;
                bestUV = tapUV;
            }
        }
    }

    return velocityMap.SampleLevel(samp, bestUV, 0).xy;
}

// Gather neighborhood mean and standard deviation for variance-based
// history clamping.  Variance clipping (mean +/- gamma*sigma) is far
// more stable than raw min/max on high-frequency textures because it
// ignores single-pixel outliers that would otherwise widen the box and
// let ghosting through, or tighten it asymmetrically and cause flicker.
void NeighborhoodStats(float2 uv, out float3 mean, out float3 sigma)
{
    float2 texel = 1.0 / ScreenSize;
    float3 m1 = 0.0;
    float3 m2 = 0.0;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float3 c = SampleCurrent(uv + float2((float) x, (float) y) * texel);
            m1 += c;
            m2 += c * c;
        }
    }

    mean = m1 / 9.0;
    float3 variance = max(m2 / 9.0 - mean * mean, 1e-6);
    sigma = sqrt(variance);
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

float3 Sharpen(float2 uv, float3 color, float strength)
{
    float2 texel = 1.0 / ScreenSize;
    float3 n = SampleCurrent(uv + float2(0.0, texel.y));
    float3 s = SampleCurrent(uv - float2(0.0, texel.y));
    float3 e = SampleCurrent(uv + float2(texel.x, 0.0));
    float3 w = SampleCurrent(uv - float2(texel.x, 0.0));
    float3 blur = (n + s + e + w) * 0.25;
    return max(color + (color - blur) * strength, 0.0);
}

PSOutput main(PSInput input)
{
    PSOutput output;
    float2 uv = input.texCoord;

    float3 current = SampleCurrent(uv);
    float3 filteredCurrent = PrefilterCurrent(uv);

    // Use dilated velocity so thin edges pick up the correct motion vector.
    float2 velocity = DilatedVelocity(uv);
    float2 prevUV = uv - velocity + JitterDeltaUV();

    // First frame or out-of-bounds reprojection starts fresh.
    bool outOfBounds = any(prevUV < 0.0) || any(prevUV > 1.0);
    if (FrameIndex == 0 || outOfBounds)
    {
        float3 start = Sharpen(uv, current, Sharpness);
        output.color = float4(start, 1.0);
        output.history = float4(start, 1.0);
        return output;
    }

    float velocityPixels = length(velocity * ScreenSize);
    float velocityFactor = saturate(velocityPixels * VelocityRejection);

    float3 history = SampleHistoryCatmullRom(prevUV);

    // Variance-based history clamping: mean +/- gamma*sigma.
    // Gamma of 1.25 keeps the box tight enough to reject ghosts while
    // being wide enough not to flicker on detailed surfaces.
    float3 mean, sigma;
    NeighborhoodStats(uv, mean, sigma);
    float gamma = lerp(ClampGamma, 0.75, velocityFactor);
    float3 clipMin = mean - sigma * gamma;
    float3 clipMax = mean + sigma * gamma;
    history = clamp(history, clipMin, clipMax);

    // Adaptive blend weight.
    float edge = EdgeFactor(uv);
    float lumaCurrent = Luma(current);
    float lumaHistory = Luma(history);
    float contrast = saturate(abs(lumaCurrent - lumaHistory) * 4.0);
    float depthCurrent = depthMap.SampleLevel(samp, uv, 0).r;
    float depthPrev = depthMap.SampleLevel(samp, prevUV, 0).r;
    float depthDelta = abs(depthCurrent - depthPrev);
    float depthFactor = saturate(depthDelta * DepthRejection);
    float disoccluded = depthDelta > DisocclusionDepthThreshold ? 1.0 : 0.0;

    float historyWeight = BaseBlend;
    historyWeight *= (1.0 - edge);
    historyWeight *= (1.0 - velocityFactor);
    historyWeight *= (1.0 - depthFactor);
    historyWeight *= (1.0 - contrast * 0.5);
    historyWeight *= (1.0 - disoccluded);
    historyWeight = clamp(historyWeight, MinBlend, MaxBlend);

    float3 resolved = lerp(current, history, historyWeight);

    // Gate sharpening by motion and disocclusion.
    float sharpenGate = 1.0 - saturate(velocityFactor + depthFactor + disoccluded);
    float sharpenStrength = Sharpness * sharpenGate;
    resolved = Sharpen(uv, resolved, sharpenStrength);

    output.color = float4(resolved, 1.0);
    output.history = float4(resolved, 1.0);
    return output;
}

