#pragma once
#include "./GPUResource.h"
#include "../Rendering/Heaps/DescriptorHeapHandle.h"
#include <DirectXMath.h>

namespace DX12Engine
{
	struct RenderTextureConfig
	{
		DirectX::XMINT3 Dimensions;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
		UINT MipLevels = 1;
		DirectX::XMFLOAT4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		bool IsCubeMap = false;
		bool ShouldResize = true;
	};

	class RenderTexture : public GPUResource
	{
	public:
		RenderTexture(ID3D12Resource* mainResource, D3D12_RESOURCE_STATES usageState, std::vector<DescriptorHeapHandle> textureDescriptors, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc, RenderTextureConfig config, bool isDepth = false);
		~RenderTexture();

		DescriptorHeapHandle GetTextureDescriptor(int index = 0) const { return m_TextureDescriptors[index]; }
		int GetTextureDescriptorCount() const { return m_TextureDescriptors.size(); }
		bool GetIsCubeMap() const { return m_Config.IsCubeMap; }
		DirectX::XMFLOAT4 GetClearColor() const { return m_Config.ClearColor; }
		const float* GetClearColorArray() const { return reinterpret_cast<const float*>(&m_Config.ClearColor); }
		DXGI_FORMAT GetFormat() const { return m_Config.Format; }
		bool IsDepth() const { return m_IsDepth; }
		RenderTextureConfig GetConfig() const { return m_Config; }

	private:
		std::vector<DescriptorHeapHandle> m_TextureDescriptors;
		RenderTextureConfig m_Config;
		bool m_IsDepth;
	};
}
