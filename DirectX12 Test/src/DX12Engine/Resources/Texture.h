#pragma once
#include <DirectXMath.h>
#include "GPUResource.h"
#include <DirectXTex.h>

namespace DX12Engine
{
	class Texture : public GPUResource
	{
	public:
		friend class Renderer;

		Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, const DirectX::TexMetadata metadata, D3D12_PLACED_SUBRESOURCE_FOOTPRINT subResourceLayouts[]);
		~Texture();

		DirectX::TexMetadata GetMetadata() const { return m_Metadata; }

	private:
		DirectX::TexMetadata m_Metadata;
		ID3D12Resource* m_MainResource;
		ID3D12Resource* m_UploadResource;
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT* m_SubResourceLayouts;
	};
}

