#pragma once
#include "d3d12.h"
#include <wrl.h>
#include <DirectXMath.h>
#include "../../Rendering/PipelineStateBuilder.h"
#include "../../Rendering/RootSignatureBuilder.h"
#include "../../Rendering/Buffers/ConstantBuffer.h"
#include "./MaterialData.h"
#include "../Texture.h"
#include <unordered_map>

namespace DX12Engine
{

	class Material
	{
	public:
		Material();
		~Material();

		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

		virtual Texture* GetTexture(TextureType type) = 0;
		virtual bool HasTexture(TextureType type) = 0;

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int cbSlot, int textureSlot);

		virtual void SetAllTextures(std::unordered_map<TextureType, std::shared_ptr<Texture>> textures) = 0;

		// Per-frame transient GPU handle for the material's texture table (set by the
		// geometry pass before issuing draws so Bind() can call SetGraphicsRootDescriptorTable).
		void SetTextureTableHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle)
		{
			m_TextureTableHandle = handle;
			m_HasTextureTable = true;
		}
		D3D12_GPU_DESCRIPTOR_HANDLE GetTextureTableHandle() const { return m_TextureTableHandle; }
		bool HasTextureTable() const { return m_HasTextureTable; }
		void ClearTextureTable() { m_HasTextureTable = false; }

		PipelineStateBuilder PipelineStateBuilder;
		RootSignatureBuilder RootSignatureBuilder;

	protected:
		explicit Material(UINT bufferSize);
		void UpdateConstantBufferData(const void* data, UINT size);

		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;

	private:
		D3D12_GPU_DESCRIPTOR_HANDLE m_TextureTableHandle{};
		bool m_HasTextureTable = false;
	};
}
