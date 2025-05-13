#pragma once
#include "d3d12.h"
#include <wrl.h>
#include <DirectXMath.h>
#include "../../Rendering/PipelineStateBuilder.h" 
#include "../../Rendering/RootSignatureBuilder.h"
#include "../../Buffers/ConstantBuffer.h"
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

		virtual void Bind(ID3D12GraphicsCommandList* commandList, int* startIndex);

		virtual void SetAllTextures(std::unordered_map<TextureType, std::shared_ptr<Texture>> textures) = 0;

		PipelineStateBuilder PipelineStateBuilder;
		RootSignatureBuilder RootSignatureBuilder;

	protected:
		void UpdateConstantBufferData(MaterialData materialData);

		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
	};
}
