#include "DepthMap.h"

namespace DX12Engine
{
	DepthMap::DepthMap(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle shaderDescriptor, DescriptorHeapHandle depthStencilDescriptor)
		: GPUResource(mainResource, usageState, shaderDescriptor)
	{
		m_DepthStencilDescriptor = depthStencilDescriptor;
	}

	DepthMap::~DepthMap()
	{
	}
}
