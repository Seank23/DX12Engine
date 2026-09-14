#ifndef CLUSTERED_LIGHTING_HLSLI
#define CLUSTERED_LIGHTING_HLSLI

#include "PBRShading.hlsli" // Light

StructuredBuffer<Light> Lights : register(t0, space1);
StructuredBuffer<uint2> ClusterGrid : register(t1, space1);
StructuredBuffer<uint> LightIndices : register(t2, space1);

#ifndef CLUSTER_DATA_REGISTER
#define CLUSTER_DATA_REGISTER b1
#endif

cbuffer ClusterData : register(CLUSTER_DATA_REGISTER)
{
	uint3 GridSize;
	uint DirectionalLightCount;
	float2 TileSizePx;
	float DepthScale;
	float DepthBias;
	uint TotalLightCount;
	uint ShowClusterHeatmap;
	float2 Padding;
}

uint GetSlice(float viewDepth)
{
	float slice = log(viewDepth) * DepthScale + DepthBias;
	return clamp((uint)slice, 0u, GridSize.z - 1u);
}

uint2 GetTile(float2 screenPos)
{
	uint2 tile = (uint2) (screenPos / TileSizePx);
	return clamp(tile, uint2(0, 0), GridSize.xy - 1u);
}

uint GetClusterIndex(float2 screenPos, float viewDepth)
{
	uint slice = GetSlice(viewDepth);
	uint2 tile = GetTile(screenPos);
	return (slice * GridSize.x * GridSize.y) + (tile.y * GridSize.x) + tile.x;
}

// Stepped on the absolute count rather than normalised against the per-cluster cap:
// a scene with a handful of lights would otherwise sit entirely in the bottom
// sixteenth of a gradient and read as one flat colour.
static const float3 ClusterHeatmapPalette[9] =
{
	float3(0.00, 0.00, 0.00), // 0 - no light reaches this froxel
	float3(0.00, 0.15, 0.80), // 1
	float3(0.00, 0.65, 0.90), // 2
	float3(0.00, 0.80, 0.25), // 3
	float3(0.85, 0.85, 0.00), // 4
	float3(1.00, 0.45, 0.00), // 5
	float3(1.00, 0.10, 0.00), // 6
	float3(1.00, 0.00, 0.75), // 7
	float3(1.00, 1.00, 1.00)  // 8+ - at or past half the per-cluster cap
};

float3 ClusterHeatmapColour(uint lightCount)
{
	return ClusterHeatmapPalette[min(lightCount, 8u)];
}

#endif // CLUSTERED_LIGHTING_HLSLI
