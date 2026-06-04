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

#endif // COLOR_UTILS_HLSLI
