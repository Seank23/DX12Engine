#pragma once
#include "./GPUResource.h"
#include "../Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class DepthMap : public GPUResource
	{
	public:
		DepthMap(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle shaderDescriptor, DescriptorHeapHandle depthStencilDescriptor);
		~DepthMap();

		DescriptorHeapHandle GetDepthStencilDescriptor() { return m_DepthStencilDescriptor; }

	private:
		DescriptorHeapHandle m_DepthStencilDescriptor;
	};
}

