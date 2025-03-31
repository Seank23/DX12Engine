#include "DepthMap.h"

namespace DX12Engine
{
	DepthMap::DepthMap(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle shaderDescriptor, std::vector<DescriptorHeapHandle> depthStencilDescriptors, bool isCubeMap)
		: GPUResource(mainResource, usageState, shaderDescriptor)
	{
		m_DepthStencilDescriptors = depthStencilDescriptors;
		m_IsCubeMap = isCubeMap;
	}

	DepthMap::~DepthMap()
	{
	}
}
