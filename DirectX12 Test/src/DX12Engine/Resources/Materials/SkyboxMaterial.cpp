#include "SkyboxMaterial.h"
#include "../ResourceManager.h"

namespace DX12Engine
{
	SkyboxMaterial::SkyboxMaterial()
	{
		D3D12_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = FALSE;  // Depth testing enabled
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Disable depth writes
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Ensure skybox is always behind objects

		PipelineStateBuilder = PipelineStateBuilder.ConfigureFromDefault().SetDepthStencilState(depthDesc)
			.SetVertexShader(ResourceManager::GetInstance().GetShader("Skybox_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("Skybox_PS"));
		RootSignatureBuilder = RootSignatureBuilder.ConfigureFromDefault(1);
		BuildPipelineState();
	}

	SkyboxMaterial::~SkyboxMaterial()
	{
	}

	void SkyboxMaterial::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex)
	{
		Material::Bind(commandList, startIndex);
		auto handle = m_Texture->GetGPUHandle();
		if (HasTexture())
			commandList->SetGraphicsRootDescriptorTable(*startIndex, m_Texture->GetGPUHandle());
		BindPipelineState(commandList);
	}
}