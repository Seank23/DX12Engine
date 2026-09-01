#ifndef GBUFFER_UTILS_HLSLI
#define GBUFFER_UTILS_HLSLI

float3 ReconstructViewPos(float2 uv, float depth, float2 screenSize, float4x4 invProjection)
{
    float4 clipPos = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    clipPos.xy += 0.5 / screenSize;
    clipPos.y *= -1.0;
    float4 viewPos = mul(invProjection, clipPos);
    return viewPos.xyz / viewPos.w;
}

float3 ReconstructWorldPos(float2 uv, float depth, float2 screenSize, float4x4 invProjection, float4x4 invView)
{
    float3 viewPos = ReconstructViewPos(uv, depth, screenSize, invProjection);
    return mul(invView, float4(viewPos, 1.0f)).xyz;
}

float2 OctEncode(float3 n)
{
    float2 oct;
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    if (n.z >= 0.0)
        oct = n.xy;
    else
        oct = (1.0 - abs(n.yx)) * select(n.xy >= 0.0, 1.0, -1.0);
    return oct;
}

float3 OctDecode(float2 oct)
{
    float3 n = float3(oct.xy, 1.0 - abs(oct.x) - abs(oct.y));
    float t = saturate(-n.z);
    n.xy += select(n.xy >= 0.0, -t, t);
    return normalize(n); // the octahedron surface is not the unit sphere
}

// Oct coordinates are [-1,1], the UNORM target stores [0,1]
float2 PackNormal(float3 n)
{
    return OctEncode(n) * 0.5 + 0.5;
}

float3 UnpackNormal(float2 stored)
{
    return OctDecode(stored * 2.0 - 1.0);
}

float4 LoadMap(float2 uv, int2 screenSize, Texture2D<float4> map)
{
    int2 pixel = clamp(int2(uv * screenSize), int2(0, 0), screenSize - 1);
    return map.Load(int3(pixel, 0));
}

float3 LoadWorldNormal(float2 uv, int2 screenSize, Texture2D<float4> packedNormals)
{
    return UnpackNormal(LoadMap(uv, screenSize, packedNormals).xy);
}

float3 SampleGeometricNormal(float2 uv, int2 screenSize, Texture2D<float4> packedNormals)
{
    return UnpackNormal(LoadMap(uv, screenSize, packedNormals).zw);
}

#endif
