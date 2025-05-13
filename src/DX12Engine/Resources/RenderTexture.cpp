#include "RenderTexture.h"

namespace DX12Engine
{
	RenderTexture::RenderTexture(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, std::vector<DescriptorHeapHandle> textureDescriptors, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, bool isCubeMap)
		: GPUResource(mainResource, usageState, srvDesc)
	{
		m_TextureDescriptors = textureDescriptors;
		m_IsCubeMap = isCubeMap;
	}

	RenderTexture::~RenderTexture()
	{
	}
}
