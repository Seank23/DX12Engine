#include "Texture.h"

namespace DX12Engine
{
	Texture::Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, std::vector<D3D12_SUBRESOURCE_DATA> data, DescriptorHeapHandle descriptor, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, bool isCubemap)
		: GPUResource(mainResource, usageState, descriptor, srvDesc)
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
