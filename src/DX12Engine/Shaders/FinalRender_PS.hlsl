#include "Common/ScreenData.hlsli"

cbuffer PostProcessingData : register(b1)
{
    int EnableGammaCorrection;
    int EnableFXAA;
    int EnableToneMapping;
    float Exposure;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

Texture2D finalRenderMap : register(t0);
SamplerState samp : register(s0);

static const float EDGE_THRESHOLD_MIN = 0.0312;
static const float EDGE_THRESHOLD_MAX = 0.25;
static const int ITERATIONS = 12;
static const float SUBPIXEL_QUALITY = 0.75;
static const float STEP_MULTIPLIER[12] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };

#include "Common/ColorUtils.hlsli"

// Sample the scene, apply exposure, tone map (ACES) and gamma-encode to display space.
// Every tap the tone-mapped image is built from goes through here, so FXAA below runs on
// the final encoded LDR image (its luma thresholds assume perceptual/gamma input) rather
// than on raw linear HDR.
float3 GradeSample(float2 texCoord)
{
    float3 color = finalRenderMap.Sample(samp, texCoord).rgb;
    color *= Exposure;
    if (EnableToneMapping != 0)
        color = ACESFitted(color);
    if (EnableGammaCorrection != 0)
        color = LinearToGamma(color);
    return color;
}

float3 applyFXAA(float3 colorCenter, float2 texCoord)
{
    float deltaX = 1.0 / ScreenSize.x;
    float deltaY = 1.0 / ScreenSize.y;
    float LumaCenter = Luma(colorCenter);
    float LumaRight = Luma(GradeSample(texCoord + float2(deltaX, 0)));
    float LumaLeft = Luma(GradeSample(texCoord + float2(-deltaX, 0)));
    float LumaDown = Luma(GradeSample(texCoord + float2(0, -deltaY)));
    float LumaUp = Luma(GradeSample(texCoord + float2(0, deltaY)));

    float LumaMin = min(LumaCenter, min(min(LumaDown, LumaUp), min(LumaLeft, LumaRight)));
    float LumaMax = max(LumaCenter, max(max(LumaDown, LumaUp), max(LumaLeft, LumaRight)));
    float LumaRange = LumaMax - LumaMin;

    if (LumaRange < max(EDGE_THRESHOLD_MIN, LumaMax * EDGE_THRESHOLD_MAX))
        return colorCenter;

    float LumaDownRight = Luma(GradeSample(texCoord + float2(deltaX, -deltaY)));
    float LumaDownLeft = Luma(GradeSample(texCoord + float2(-deltaX, -deltaY)));
    float LumaUpRight = Luma(GradeSample(texCoord + float2(deltaX, deltaY)));
    float LumaUpLeft = Luma(GradeSample(texCoord + float2(-deltaX, deltaY)));
    float LumaDownUp = LumaDown + LumaUp;
    float LumaLeftRight = LumaLeft + LumaRight;
    float LumaLeftCorners = LumaDownLeft + LumaUpLeft;
    float LumaRightCorners = LumaDownRight + LumaUpRight;
    float LumaDownCorners = LumaDownLeft + LumaDownRight;
    float LumaUpCorners = LumaUpLeft + LumaUpRight;
    float edgeHorizontal = abs(-2.0 * LumaLeft + LumaLeftCorners) + abs(-2.0 * LumaCenter + LumaDownUp) * 2.0 + abs(-2.0 * LumaRight + LumaRightCorners);
    float edgeVertical = abs(-2.0 * LumaUp + LumaUpCorners) + abs(-2.0 * LumaCenter + LumaLeftRight) * 2.0 + abs(-2.0 * LumaDown + LumaDownCorners);

    bool isHorizontal = edgeHorizontal >= edgeVertical;

    float Luma1 = isHorizontal ? LumaDown : LumaLeft;
    float Luma2 = isHorizontal ? LumaUp : LumaRight;
    float gradient1 = Luma1 - LumaCenter;
    float gradient2 = Luma2 - LumaCenter;
    float is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2)) / max(LumaRange, 1e-5);

    float stepLength = isHorizontal ? deltaY : deltaX;
    float LumaLocalAverage = 0.0;
    if (is1Steepest)
    {
        stepLength = -stepLength;
        LumaLocalAverage = 0.5 * (Luma1 + LumaCenter);
    }
    else
    {
        LumaLocalAverage = 0.5 * (Luma2 + LumaCenter);
    }
    float2 currentTexCoord = texCoord;
    isHorizontal ? currentTexCoord.y += stepLength * 0.5 : currentTexCoord.x += stepLength * 0.5;

    float2 offset = isHorizontal ? float2(deltaX, 0) : float2(0, deltaY);
    float2 uv1 = currentTexCoord - offset;
    float2 uv2 = currentTexCoord + offset;
    float LumaEnd1 = Luma(GradeSample(uv1));
    float LumaEnd2 = Luma(GradeSample(uv2));
    LumaEnd1 -= LumaLocalAverage;
    LumaEnd2 -= LumaLocalAverage;
    bool reached1 = abs(LumaEnd1) >= gradientScaled;
    bool reached2 = abs(LumaEnd2) >= gradientScaled;
    bool reachedBoth = reached1 && reached2;
    if (!reached1)
        uv1 -= offset;
    if (!reached2)
        uv2 += offset;

    if (!reachedBoth)
    {
        for (int i = 2; i < ITERATIONS; i++)
        {
            if (!reached1)
                LumaEnd1 = Luma(GradeSample(uv1)) - LumaLocalAverage;
            if (!reached2)
                LumaEnd2 = Luma(GradeSample(uv2)) - LumaLocalAverage;

            reached1 = abs(LumaEnd1) >= gradientScaled;
            reached2 = abs(LumaEnd2) >= gradientScaled;
            reachedBoth = reached1 && reached2;

            if (reachedBoth)
                break;
            if (!reached1)
                uv1 -= offset * STEP_MULTIPLIER[min(i, 11)];
            if (!reached2)
                uv2 += offset * STEP_MULTIPLIER[min(i, 11)];
        }
    }

    float distance1 = isHorizontal ? abs(currentTexCoord.y - uv1.y) : abs(currentTexCoord.x - uv1.x);
    float distance2 = isHorizontal ? abs(currentTexCoord.y - uv2.y) : abs(currentTexCoord.x - uv2.x);
    bool isDirection1 = distance1 < distance2;
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = distance1 + distance2;
    float pixelOffset = -distanceFinal / max(edgeThickness, 1e-5) + 0.5;

    bool isLumaCenterSmaller = LumaCenter < LumaLocalAverage;
    bool correctVariation = ((isDirection1 ? LumaEnd1 : LumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;

    float LumaAverage = (1.0 / 9.0) * (2.0 * (LumaDownUp + LumaLeftRight) + LumaLeftCorners + LumaRightCorners);
    float subPixelOffset1 = clamp(abs(LumaAverage - LumaCenter) / LumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
    finalOffset = max(finalOffset, subPixelOffsetFinal);

    float2 finalUv = texCoord;
    isHorizontal ? finalUv.y += finalOffset * stepLength : finalUv.x += finalOffset * stepLength;

    return GradeSample(finalUv);
}

float4 main(PSInput input) : SV_TARGET
{
    // Scene is resolved in linear HDR; grade (exposure -> tone map -> gamma) first, then
    // optionally anti-alias the encoded result.
    float3 colorCenter = GradeSample(input.texCoord);

    float3 finalColor = colorCenter;
    if (EnableFXAA != 0)
        finalColor = applyFXAA(colorCenter, input.texCoord);
    return float4(finalColor, 1.0);
}
