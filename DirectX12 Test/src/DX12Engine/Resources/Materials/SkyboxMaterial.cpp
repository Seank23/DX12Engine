#include "SkyboxMaterial.h"
#include "../ResourceManager.h"

namespace DX12Engine
{
	SkyboxMaterial::SkyboxMaterial()
	{
		D3D12_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = true;  // Depth testing enabled
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Disable depth writes
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Ensure skybox is always behind objects
		depthDesc.StencilEnable = false;

		D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT; // Cull front faces instead of back
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

		PipelineStateBuilder = PipelineStateBuilder.ConfigureFromDefault().SetDepthStencilState(depthDesc).SetRasterizerState(rasterizerDesc)
			.SetVertexShader(ResourceManager::GetInstance().GetShader("Skybox_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("Skybox_PS"));
		RootSignatureBuilder = RootSignatureBuilder.ConfigureFromDefault(1);
		BuildPipelineState();
	}

	SkyboxMaterial::~SkyboxMaterial()
	{
	}

	void SkyboxMaterial::Bind(ID3D12GraphicsCommandList* commandList, int* startIndex, bool bindPipelineState)
	{
		Material::Bind(commandList, startIndex, bindPipelineState);
		if (HasTexture())
			commandList->SetGraphicsRootDescriptorTable(*startIndex, m_Texture->GetGPUHandle());
	}
}