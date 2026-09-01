#ifndef SCREEN_DATA_HLSLI
#define SCREEN_DATA_HLSLI

#include "GBufferUtils.hlsli"

#ifndef SCREEN_DATA_REGISTER
#define SCREEN_DATA_REGISTER b0 // PBRTransparent_PS defines b1 before including; its b0 is ObjectData
#endif

// Mirrors ScreenData in Rendering/RenderPass/RenderPassData.h -- keep the two in step.
cbuffer ScreenData : register(SCREEN_DATA_REGISTER)
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

// Convenience overloads that close over the cbuffer, so call sites do not repeat it.
float3 ReconstructViewPos(float2 uv, float depth)
{
    return ReconstructViewPos(uv, depth, ScreenSize, InvProjectionMatrix);
}

float3 ReconstructWorldPos(float2 uv, float depth)
{
    return ReconstructWorldPos(uv, depth, ScreenSize, InvProjectionMatrix, InvViewMatrix);
}

float4 LoadMap(float2 uv, Texture2D<float4> map)
{
    return LoadMap(uv, int2(ScreenSize), map);
}

float3 LoadWorldNormal(float2 uv, Texture2D<float4> packedNormals)
{
    return LoadWorldNormal(uv, int2(ScreenSize), packedNormals);
}

float3 SampleGeometricNormal(float2 uv, Texture2D<float4> packedNormals)
{
    return SampleGeometricNormal(uv, int2(ScreenSize), packedNormals);
}

#endif
