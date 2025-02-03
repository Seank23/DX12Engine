#include "Texture.h"

namespace DX12Engine
{
	Texture::Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, D3D12_SUBRESOURCE_DATA data, DescriptorHeapHandle srvHandle)
		: GPUResource(mainResource, usageState)
	{
		m_MainResource = mainResource;
		m_UploadResource = uploadResource;
		m_Data = data;
		m_SRVHandle = srvHandle;
	}

	Texture::~Texture()
	{
	}
}
