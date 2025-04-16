#include "RenderTexture.h"

namespace DX12Engine
{
	RenderTexture::RenderTexture(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle shaderDescriptor, std::vector<DescriptorHeapHandle> depthStencilDescriptors, bool isCubeMap)
		: GPUResource(mainResource, usageState, shaderDescriptor)
	{
		m_TextureDescriptors = depthStencilDescriptors;
		m_IsCubeMap = isCubeMap;
	}

	RenderTexture::~RenderTexture()
	{
	}
}
