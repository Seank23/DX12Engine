#ifndef COLOR_UTILS_HLSLI
#define COLOR_UTILS_HLSLI

float3 sRGBToLinear(float3 color)
{
    return pow(max(color, 0.0), 2.2);
}

float Luma(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float Max3(float3 value)
{
    return max(value.x, max(value.y, value.z));
}

// Encode scene-linear HDR to display gamma (~sRGB). Matches sRGBToLinear's 2.2 convention.
float3 LinearToGamma(float3 color)
{
    return pow(max(color, 0.0), 1.0 / 2.2);
}

// ---- ACES filmic tone mapping (Stephen Hill's fitted RRT+ODT) ----
// Fits the ACES Reference Rendering Transform + sRGB Output Device Transform with a
// pair of 3x3 colour-space transforms around a rational curve. Compared to the cheaper
// Narkowicz luminance-only fit, this preserves hue far better and avoids the magenta
// highlight shift, at the cost of two extra matrix multiplies. Input is scene-linear HDR.
static const float3x3 ACESInputMat =
{
    { 0.59719, 0.35458, 0.04823 },
    { 0.07600, 0.90834, 0.01566 },
    { 0.02840, 0.13383, 0.83777 }
};

static const float3x3 ACESOutputMat =
{
    {  1.60475, -0.53108, -0.07367 },
    { -0.10208,  1.10813, -0.00605 },
    { -0.00327, -0.07276,  1.07602 }
};

float3 RRTAndODTFit(float3 v)
{
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

float3 ACESFitted(float3 color)
{
    color = mul(ACESInputMat, color);
    color = RRTAndODTFit(color);
    color = mul(ACESOutputMat, color);
    return saturate(color);
}

#endif // COLOR_UTILS_HLSLI
