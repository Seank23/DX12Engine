#include "Texture.h"

namespace DX12Engine
{
	Texture::Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, std::vector<D3D12_SUBRESOURCE_DATA> data, DescriptorHeapHandle descriptor, bool isCubemap)
		: GPUResource(mainResource, usageState, descriptor)
	{
		m_MainResource = mainResource;
		m_UploadResource = uploadResource;
		m_Data = data;
		m_IsCubemap = isCubemap;
	}

	Texture::~Texture()
	{
	}
}
