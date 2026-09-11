#pragma once
#include "../../Utils/Constants.h"

#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>

namespace DX12Engine
{
	class Camera;
	class LightBuffer;

	class ClusteredLightGrid
	{
	public:
		static int GetClusterIndex(int x, int y, int z)
		{
			return x + y * LIGHT_CLUSTER_GRID_X + z * LIGHT_CLUSTER_GRID_X * LIGHT_CLUSTER_GRID_Y;
		}

		static bool SphereIntersectsAABB(DirectX::FXMVECTOR centre, float radius, const DirectX::XMFLOAT4& aabbMin, const DirectX::XMFLOAT4& aabbMax)
		{
			DirectX::XMVECTOR bMin = DirectX::XMLoadFloat4(&aabbMin);
			DirectX::XMVECTOR bMax = DirectX::XMLoadFloat4(&aabbMax);
			DirectX::XMVECTOR zero = DirectX::XMVectorZero();
			DirectX::XMVECTOR d = DirectX::XMVectorAdd(
				DirectX::XMVectorMax(zero, DirectX::XMVectorSubtract(bMin, centre)),
				DirectX::XMVectorMax(zero, DirectX::XMVectorSubtract(centre, bMax)));
			return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(d)) <= radius * radius;
		}

		struct LightGridFrameAddresses
		{
			D3D12_GPU_VIRTUAL_ADDRESS ClusterData = 0;
			D3D12_GPU_VIRTUAL_ADDRESS Lights = 0;
			D3D12_GPU_VIRTUAL_ADDRESS Grid = 0;
			D3D12_GPU_VIRTUAL_ADDRESS LightIndices = 0;
		};

		float GetViewZ(float k) { return m_CachedNear * powf(m_CachedFar / m_CachedNear, k / LIGHT_CLUSTER_GRID_Z); }
		int GetSlice(float viewZ) { return static_cast<int>(floorf(m_DepthScale * logf(viewZ) + m_DepthBias)); }

		LightGridFrameAddresses Build(const Camera& camera, const LightBuffer& lightBuffer, DirectX::XMINT2 renderSize, bool showHeatMap);

		uint32_t GetPeakLightsPerCluster() const { return m_PeakLightsPerCluster; }
		uint32_t GetDroppedLightCount() const { return m_DroppedLightCount; }
		// How much of the flat index list the frame actually used. Peak occupancy says how
		// bad the worst cluster is; this says how close the whole grid is to dropping.
		uint32_t GetLightIndexCount() const { return static_cast<uint32_t>(m_LightIndices.size()); }
		uint32_t GetFroxelRebuildCount() const { return m_FroxelRebuildCount; }

	private:
		struct ClusterRange
		{
			uint32_t Offset;
			uint32_t Count;
		};

		void RebuildFroxelBounds(const Camera& camera);
		void FlattenBuckets();

		std::vector<DirectX::XMFLOAT4> m_FroxelMin;
		std::vector<DirectX::XMFLOAT4> m_FroxelMax;
		std::vector<ClusterRange> m_Grid;
		std::vector<uint32_t> m_LightIndices;
		std::vector<std::vector<uint32_t>> m_Buckets;
		float m_CachedFov = 0.0f, m_CachedAspect = 0.0f, m_CachedNear = 0.0f, m_CachedFar = 0.0f, m_DepthScale = 0.0f, m_DepthBias = 0.0f;
		uint32_t m_PeakLightsPerCluster = 0, m_DroppedLightCount = 0, m_FroxelRebuildCount = 0;
	};
}