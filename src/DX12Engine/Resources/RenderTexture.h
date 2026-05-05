#pragma once
#include "./GPUResource.h"
#include "../Rendering/Heaps/DescriptorHeapHandle.h"
#include <DirectXMath.h>

namespace DX12Engine
{
	class RenderTexture : public GPUResource
	{
	public:
		RenderTexture(ID3D12Resource* mainResource, DXGI_FORMAT format, D3D12_RESOURCE_STATES usageState, std::vector<DescriptorHeapHandle> textureDescriptors, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, bool isCubeMap = false, DirectX::XMFLOAT4 clearColor = { 0.0f, 0.0f, 0.0f, 1.0f });
		~RenderTexture();

		DescriptorHeapHandle GetTextureDescriptor(int index = 0) { return m_TextureDescriptors[index]; }
		int GetTextureDescriptorCount() { return m_TextureDescriptors.size(); }
		bool GetIsCubeMap() { return m_IsCubeMap; }
		DirectX::XMFLOAT4 GetClearColor() { return m_ClearColor; }
		DXGI_FORMAT GetFormat() { return m_Format; }

	private:
		std::vector<DescriptorHeapHandle> m_TextureDescriptors;
		bool m_IsCubeMap;
		DirectX::XMFLOAT4 m_ClearColor;
		DXGI_FORMAT m_Format;
	};
}

