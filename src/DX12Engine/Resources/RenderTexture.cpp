#include "RenderTexture.h"

namespace DX12Engine
{
	RenderTexture::RenderTexture(ID3D12Resource* mainResource, DXGI_FORMAT format, D3D12_RESOURCE_STATES usageState, std::vector<DescriptorHeapHandle> textureDescriptors, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, bool isCubeMap, DirectX::XMFLOAT4 clearColor)
		: GPUResource(mainResource, usageState, srvDesc)
	{
		m_Format = format;
		m_TextureDescriptors = textureDescriptors;
		m_IsCubeMap = isCubeMap;
		m_ClearColor = clearColor;
	}

	RenderTexture::~RenderTexture()
	{
	}
}
