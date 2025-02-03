#pragma once
#include <DirectXMath.h>
#include "GPUResource.h"
#include "../Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class Texture : public GPUResource
	{
	public:
		friend class Renderer;

		Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, D3D12_SUBRESOURCE_DATA data, DescriptorHeapHandle srvHandle);
		~Texture();

		DescriptorHeapHandle GetSRVHandle() { return m_SRVHandle; }

	private:
		ID3D12Resource* m_MainResource;
		ID3D12Resource* m_UploadResource;
		D3D12_SUBRESOURCE_DATA m_Data;
		DescriptorHeapHandle m_SRVHandle;
	};
}

