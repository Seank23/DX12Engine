#include "Texture.h"

namespace DX12Engine
{
	Texture::Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, const DirectX::TexMetadata metadata, D3D12_PLACED_SUBRESOURCE_FOOTPRINT subResourceLayouts[])
		: GPUResource(mainResource, usageState)
	{
		m_MainResource = mainResource;
		m_UploadResource = uploadResource;
		m_Metadata = metadata;
		m_SubResourceLayouts = subResourceLayouts;
	}

	Texture::~Texture()
	{
	}
}
