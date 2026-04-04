cbuffer ScreenData : register(b0)
{
    float4 CameraPosition;
    float4x4 ViewMatrix;
    float4x4 ProjectionMatrix;
    float4x4 InvViewMatrix;
    float4x4 InvProjectionMatrix;
    float2 ScreenSize;
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

float rgbToLuminance(float3 color)
{
    return sqrt(dot(color, float3(0.299, 0.587, 0.114))); // Standard luminance conversion
}

float3 applyFXAA(float3 colorCenter, float2 texCoord)
{
    float deltaX = 1.0 / ScreenSize.x;
    float deltaY = 1.0 / ScreenSize.y;
    float lumaCenter = rgbToLuminance(colorCenter);
    float lumaRight = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(deltaX, 0)).rgb);
    float lumaLeft = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(-deltaX, 0)).rgb);
    float lumaDown = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(0, -deltaY)).rgb);
    float lumaUp = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(0, deltaY)).rgb);
    
    float lumaMin = min(lumaCenter, min(min(lumaDown, lumaUp), min(lumaLeft, lumaRight)));
    float lumaMax = max(lumaCenter, max(max(lumaDown, lumaUp), max(lumaLeft, lumaRight)));
    float lumaRange = lumaMax - lumaMin;
    
    if (lumaRange < max(EDGE_THRESHOLD_MIN, lumaMax * EDGE_THRESHOLD_MAX))
        return colorCenter;
    
    float lumaDownRight = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(deltaX, -deltaY)).rgb);
    float lumaDownLeft = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(-deltaX, -deltaY)).rgb);
    float lumaUpRight = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(deltaX, deltaY)).rgb);
    float lumaUpLeft = rgbToLuminance(finalRenderMap.Sample(samp, texCoord + float2(-deltaX, deltaY)).rgb);
    float lumaDownUp = lumaDown + lumaUp;
    float lumaLeftRight = lumaLeft + lumaRight;
    float lumaLeftCorners = lumaDownLeft + lumaUpLeft;
    float lumaRightCorners = lumaDownRight + lumaUpRight;
    float lumaDownCorners = lumaDownLeft + lumaDownRight;
    float lumaUpCorners = lumaUpLeft + lumaUpRight;
    float edgeHorizontal = abs(-2.0 * lumaLeft + lumaLeftCorners) + abs(-2.0 * lumaCenter + lumaDownUp) * 2.0 + abs(-2.0 * lumaRight + lumaRightCorners);
    float edgeVertical = abs(-2.0 * lumaUp + lumaUpCorners) + abs(-2.0 * lumaCenter + lumaLeftRight) * 2.0 + abs(-2.0 * lumaDown + lumaDownCorners);
    
    bool isHorizontal = edgeHorizontal >= edgeVertical;
    
    float luma1 = isHorizontal ? lumaDown : lumaLeft;
    float luma2 = isHorizontal ? lumaUp : lumaRight;
    float gradient1 = luma1 - lumaCenter;
    float gradient2 = luma2 - lumaCenter;
    float is1Steepest = abs(gradient1) >= abs(gradient2);
    float gradientScaled = 0.25 * max(abs(gradient1), abs(gradient2)) / max(lumaRange, 1e-5);
    
    float stepLength = isHorizontal ? deltaY : deltaX;
    float lumaLocalAverage = 0.0;
    if (is1Steepest)
    {
        stepLength = -stepLength;
        lumaLocalAverage = 0.5 * (luma1 + lumaCenter);
    }
    else
    {
        lumaLocalAverage = 0.5 * (luma2 + lumaCenter);
    }
    float2 currentTexCoord = texCoord;
    isHorizontal ? currentTexCoord.y += stepLength * 0.5 : currentTexCoord.x += stepLength * 0.5;
    
    float2 offset = isHorizontal ? float2(deltaX, 0) : float2(0, deltaY);
    float2 uv1 = currentTexCoord - offset;
    float2 uv2 = currentTexCoord + offset;
    float lumaEnd1 = rgbToLuminance(finalRenderMap.Sample(samp, uv1).rgb);
    float lumaEnd2 = rgbToLuminance(finalRenderMap.Sample(samp, uv2).rgb);
    lumaEnd1 -= lumaLocalAverage;
    lumaEnd2 -= lumaLocalAverage;
    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;
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
                lumaEnd1 = rgbToLuminance(finalRenderMap.Sample(samp, uv1).rgb) - lumaLocalAverage;
            if (!reached2)
                lumaEnd2 = rgbToLuminance(finalRenderMap.Sample(samp, uv2).rgb) - lumaLocalAverage;
            
            reached1 = abs(lumaEnd1) >= gradientScaled;
            reached2 = abs(lumaEnd2) >= gradientScaled;
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
    
    bool isLumaCenterSmaller = lumaCenter < lumaLocalAverage;
    bool correctVariation = ((isDirection1 ? lumaEnd1 : lumaEnd2) < 0.0) != isLumaCenterSmaller;
    float finalOffset = correctVariation ? pixelOffset : 0.0;
    
    float lumaAverage = (1.0 / 9.0) * (2.0 * (lumaDownUp + lumaLeftRight) + lumaLeftCorners + lumaRightCorners);
    float subPixelOffset1 = clamp(abs(lumaAverage - lumaCenter) / lumaRange, 0.0, 1.0);
    float subPixelOffset2 = (-2.0 * subPixelOffset1 + 3.0) * subPixelOffset1 * subPixelOffset1;
    float subPixelOffsetFinal = subPixelOffset2 * subPixelOffset2 * SUBPIXEL_QUALITY;
    finalOffset = max(finalOffset, subPixelOffsetFinal);
    
    float2 finalUv = texCoord;
    isHorizontal ? finalUv.y += finalOffset * stepLength : finalUv.x += finalOffset * stepLength;
    
    return finalRenderMap.Sample(samp, finalUv).rgb;
}

float4 main(PSInput input) : SV_TARGET
{
    float3 colorCenter = finalRenderMap.Sample(samp, input.texCoord).rgb;
    
    float3 finalColor = colorCenter;
    finalColor = applyFXAA(colorCenter, input.texCoord);
    finalColor = pow(finalColor, 1.0 / 2.2); // Gamma correction
    return float4(finalColor, 1.0);
}