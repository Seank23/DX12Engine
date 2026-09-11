#define NOMINMAX
#include "ClusteredLightGrid.h"
#include "../../Input/Camera.h"
#include "../../Utils/Constants.h"
#include "../Buffers/LightBuffer.h"
#include "../../Resources/ResourceManager.h"

#include <algorithm>

namespace DX12Engine
{
	ClusteredLightGrid::LightGridFrameAddresses ClusteredLightGrid::Build(const Camera& camera, const LightBuffer& lightBuffer, DirectX::XMINT2 renderSize, bool showHeatMap)
	{
		m_Grid.resize(LIGHT_CLUSTER_COUNT);
		m_Buckets.resize(LIGHT_CLUSTER_COUNT);
		m_LightIndices.reserve(MAX_LIGHT_INDICES);
		RebuildFroxelBounds(camera);
		for (auto& bucket : m_Buckets)
			bucket.clear();

		const auto& lights = lightBuffer.GetUploadData();
		uint32_t directionalLightCount = lightBuffer.GetDirectionalCount();
		for (int i = directionalLightCount; i < lights.size(); ++i)
		{
			const auto& light = lights[i];
			DirectX::XMVECTOR lightViewPos = DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&light.Position), camera.GetViewMatrix());
			float radius = light.Range;

			if (DirectX::XMVectorGetZ(lightViewPos) + radius <= m_CachedNear)
				continue;
			int sliceLow = std::clamp(GetSlice(std::max(DirectX::XMVectorGetZ(lightViewPos) - radius, m_CachedNear)), 0, (int)LIGHT_CLUSTER_GRID_Z - 1);
			int sliceHigh = std::clamp(GetSlice(std::min(DirectX::XMVectorGetZ(lightViewPos) + radius, m_CachedFar)), 0, (int)LIGHT_CLUSTER_GRID_Z - 1);

			for (int k = sliceLow; k <= sliceHigh; ++k)
			{
				for (int y = 0; y < LIGHT_CLUSTER_GRID_Y; ++y)
				{
					for (int x = 0; x < LIGHT_CLUSTER_GRID_X; ++x)
					{
						int clusterIdx = GetClusterIndex(x, y, k);
						if (SphereIntersectsAABB(lightViewPos, radius, m_FroxelMin[clusterIdx], m_FroxelMax[clusterIdx]))
							m_Buckets[clusterIdx].push_back(i);
					}
				}
			}
		}

		FlattenBuckets();

		ClusterData clusterData{};
		clusterData.GridSize = DirectX::XMUINT3(LIGHT_CLUSTER_GRID_X, LIGHT_CLUSTER_GRID_Y, LIGHT_CLUSTER_GRID_Z);
		clusterData.DirectionalLightCount = directionalLightCount;
		clusterData.TileSizePx = DirectX::XMFLOAT2((float)renderSize.x / LIGHT_CLUSTER_GRID_X, (float)renderSize.y / LIGHT_CLUSTER_GRID_Y);
		clusterData.DepthScale = m_DepthScale;
		clusterData.DepthBias = m_DepthBias;
		clusterData.TotalLightCount = (uint32_t)lights.size();
		clusterData.ShowClusterHeatmap = showHeatMap ? 1 : 0;

		auto clusterDataAddress = ResourceManager::GetInstance().AllocateFrameConstants(&clusterData, sizeof(ClusterData));
		auto gridAddress = ResourceManager::GetInstance().AllocateFrameConstants(m_Grid.data(), static_cast<UINT>(m_Grid.size() * sizeof(ClusterRange)));
		auto lightIndicesAddress = ResourceManager::GetInstance().AllocateFrameConstants(m_LightIndices.data(), static_cast<UINT>(m_LightIndices.size() * sizeof(uint32_t)));
		auto lightsAddress = ResourceManager::GetInstance().AllocateFrameConstants(lights.data(), static_cast<UINT>(lights.size() * sizeof(LightData)));

		return { clusterDataAddress, lightsAddress, gridAddress, lightIndicesAddress };
	}

	void ClusteredLightGrid::RebuildFroxelBounds(const Camera& camera)
	{
		if (m_CachedFov == camera.GetFOV() && m_CachedAspect == camera.GetAspectRatio() && m_CachedNear == camera.GetNearPlane() && m_CachedFar == camera.GetFarPlane())
			return;

		m_CachedFov = camera.GetFOV();
		m_CachedAspect = camera.GetAspectRatio();
		m_CachedNear = camera.GetNearPlane();
		m_CachedFar = camera.GetFarPlane();
		m_FroxelRebuildCount++;

		m_FroxelMin.resize(LIGHT_CLUSTER_COUNT);
		m_FroxelMax.resize(LIGHT_CLUSTER_COUNT);

		float tanHalfFov = tanf(DirectX::XMConvertToRadians(m_CachedFov) * 0.5f);
		float xScale = tanHalfFov * m_CachedAspect;
		float yScale = tanHalfFov;
		float depthRatio = m_CachedFar / m_CachedNear;

		m_DepthScale = LIGHT_CLUSTER_GRID_Z / logf(depthRatio);
		m_DepthBias = -m_DepthScale * logf(m_CachedNear);

		for (int k = 0; k < LIGHT_CLUSTER_GRID_Z; ++k)
		{
			float zNear = m_CachedNear * powf(depthRatio, (float)k / LIGHT_CLUSTER_GRID_Z);
			float zFar = m_CachedNear * powf(depthRatio, (float)(k + 1) / LIGHT_CLUSTER_GRID_Z);

			for (int y = 0; y < LIGHT_CLUSTER_GRID_Y; ++y)
			{
				float ndcMaxY = 1.0f - 2.0f * (float)y / LIGHT_CLUSTER_GRID_Y;
				float ndcMinY = 1.0f - 2.0f * (float)(y + 1) / LIGHT_CLUSTER_GRID_Y;

				for (int x = 0; x < LIGHT_CLUSTER_GRID_X; ++x)
				{
					float ndcMinX = 2.0f * (float)x / LIGHT_CLUSTER_GRID_X - 1.0f;
					float ndcMaxX = 2.0f * (float)(x + 1) / LIGHT_CLUSTER_GRID_X - 1.0f;

					float xs[4] = { ndcMinX * xScale * zNear, ndcMinX * xScale * zFar, ndcMaxX * xScale * zNear, ndcMaxX * xScale * zFar };
					float ys[4] = { ndcMinY * yScale * zNear, ndcMinY * yScale * zFar, ndcMaxY * yScale * zNear, ndcMaxY * yScale * zFar };

					int clusterIdx = GetClusterIndex(x, y, k);

					m_FroxelMin[clusterIdx] = DirectX::XMFLOAT4(
						std::min(xs[0], std::min(xs[1], std::min(xs[2], xs[3]))),
						std::min(ys[0], std::min(ys[1], std::min(ys[2], ys[3]))),
						zNear,
						0.0f);
					m_FroxelMax[clusterIdx] = DirectX::XMFLOAT4(
						std::max(xs[0], std::max(xs[1], std::max(xs[2], xs[3]))),
						std::max(ys[0], std::max(ys[1], std::max(ys[2], ys[3]))),
						zFar,
						0.0f);
				}
			}
		}
	}

	void ClusteredLightGrid::FlattenBuckets()
	{
		m_LightIndices.clear();
		m_PeakLightsPerCluster = 0;
		m_DroppedLightCount = 0;

		int offset = 0;
		for (int i = 0; i < LIGHT_CLUSTER_COUNT; ++i)
		{
			size_t demand = m_Buckets[i].size();
			m_PeakLightsPerCluster = std::max(m_PeakLightsPerCluster, (uint32_t)demand);
			size_t remaining = MAX_LIGHT_INDICES - offset;
			size_t count = std::min(demand, remaining);

			m_Grid[i] = { (uint32_t)offset, (uint32_t)count };
			for (size_t j = 0; j < count; ++j)
				m_LightIndices.push_back(m_Buckets[i][j]);
			offset += count;
			m_DroppedLightCount += (uint32_t)(demand - count);
		}
	}
}