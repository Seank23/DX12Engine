#pragma once
#include "./GPUResource.h"
#include "../Rendering/Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class RenderTexture : public GPUResource
	{
	public:
		RenderTexture(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, std::vector<DescriptorHeapHandle> textureDescriptors, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, bool isCubeMap = false);
		~RenderTexture();

		DescriptorHeapHandle GetTextureDescriptor(int index = 0) { return m_TextureDescriptors[index]; }
		int GetTextureDescriptorCount() { return m_TextureDescriptors.size(); }
		bool GetIsCubeMap() { return m_IsCubeMap; }

	private:
		std::vector<DescriptorHeapHandle> m_TextureDescriptors;
		bool m_IsCubeMap;
	};
}

