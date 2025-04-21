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

		PipelineStateBuilder = PipelineStateBuilder.AddInputLayout()
			.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(rasterizerDesc)
			.SetDepthStencilState(depthDesc)
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0)
			.SetVertexShader(ResourceManager::GetInstance().GetShader("Skybox_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("Skybox_PS"));

		DescriptorTableConfig skybox(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		RootSignatureBuilder = RootSignatureBuilder.AddConstantBuffer(0).AddConstantBuffer(1)
			.AddDescriptorTables({ skybox })
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC);
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