#pragma once
#include <DirectXMath.h>
#include "GPUResource.h"
#include "../Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	enum TextureType
	{
		Albedo,
		Normal,
		Metallic,
		Roughness,
		AOMap
	};

	class Texture : public GPUResource
	{
	public:
		friend class GPUUploader;

		Texture(ID3D12Resource* mainResource, ID3D12Resource* uploadResource, D3D12_RESOURCE_STATES usageState, std::vector<D3D12_SUBRESOURCE_DATA> data, DescriptorHeapHandle descriptor, bool isCubemap);
		~Texture();

		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() { return GetDescriptor()->GetGPUHandle(); }
		bool IsCubemap() { return m_IsCubemap; }

	private:
		ID3D12Resource* m_MainResource;
		ID3D12Resource* m_UploadResource;
		std::vector<D3D12_SUBRESOURCE_DATA> m_Data;
		bool m_IsCubemap = false;
	};
}

