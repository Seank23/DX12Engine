#include "MaterialTemplate.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	void MaterialTemplate::ResolvePSO()
	{
		if (m_PipelineState)
			return;

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(BuildRootSignatureDesc());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = BuildPSODesc();
		psoDesc.pRootSignature = m_RootSignature.Get();
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(psoDesc);
	}

	D3D12_ROOT_SIGNATURE_DESC MaterialTemplate::BuildRootSignatureDesc()
	{
		m_RootSignatureBuilder = RootSignatureBuilder{};

		// Geometry:
		//   b0 object, b1 material, t0..t5 material textures
		// Transparent:
		//   b0 object, b1 material, t0..t5 material textures, t6 opaque scene, t7 env map, t8 depth
		DescriptorTableConfig materialTexTable(6, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		m_RootSignatureBuilder.AddConstantBuffer(0).AddConstantBuffer(1);

		if (m_PassTarget == PassTarget::Transparent)
		{
			DescriptorTableConfig envMapTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6);
			DescriptorTableConfig opaqueSceneTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7);
			DescriptorTableConfig depthTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8);
			m_RootSignatureBuilder
				.AddConstantBuffer(2)
				.AddDescriptorTables({ materialTexTable, envMapTable, opaqueSceneTable, depthTable })
				.AddSampler(0, D3D12_FILTER_ANISOTROPIC)
				.AddSampler(1, D3D12_FILTER_ANISOTROPIC);
		}
		else
		{
			m_RootSignatureBuilder
				.AddDescriptorTables({ materialTexTable })
				.AddSampler(0, D3D12_FILTER_ANISOTROPIC);
		}

		return m_RootSignatureBuilder.Build();
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC MaterialTemplate::BuildPSODesc()
	{
		if (m_PassTarget == PassTarget::Transparent)
		{
			m_VertexShaderName = "PBRTransparent_VS";
			m_PixelShaderName = "PBRTransparent_PS";
		}
		else
		{
			m_VertexShaderName = "Geometry_VS";
			m_PixelShaderName = "Geometry_PS";
		}

		Shader* vs = ResourceManager::GetInstance().GetShader(m_VertexShaderName);
		Shader* ps = ResourceManager::GetInstance().GetShader(m_PixelShaderName);

		D3D12_RASTERIZER_DESC rastDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rastDesc.CullMode = m_RasterizerPolicy.CullMode;
		rastDesc.FillMode = m_RasterizerPolicy.FillMode;
		rastDesc.DepthBias = m_RasterizerPolicy.DepthBias;
		rastDesc.SlopeScaledDepthBias = m_RasterizerPolicy.SlopeScaledDepthBias;

		D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		if (m_BlendPolicy == AlphaMode::Blend)
		{
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}

		m_PipelineStateBuilder = PipelineStateBuilder{};
		m_PipelineStateBuilder.ConfigureFromDefault(vs, ps)
			.SetRasterizerState(rastDesc)
			.SetBlendState(blendDesc);

		if (m_PassTarget == PassTarget::Transparent)
		{
			D3D12_DEPTH_STENCIL_DESC depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			if (m_BlendPolicy == AlphaMode::Blend)
			{
				depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				depthDesc.DepthEnable = FALSE;
			}

			m_PipelineStateBuilder
				.SetDepthStencilState(depthDesc)
				.SetRenderTargets({ DXGI_FORMAT_R16G16B16A16_FLOAT });
		}
		else
		{
			m_PipelineStateBuilder.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM,
													  DXGI_FORMAT_R16G16B16A16_FLOAT,
													  DXGI_FORMAT_R16G16B16A16_FLOAT,
													  DXGI_FORMAT_R16G16B16A16_FLOAT,
													  DXGI_FORMAT_R16G16B16A16_FLOAT,
													  DXGI_FORMAT_R16G16B16A16_FLOAT,
													  DXGI_FORMAT_R16G16_FLOAT })
				.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT);
		}

		return m_PipelineStateBuilder.Build();
	}

	void MaterialTemplate::InvalidateCache()
	{
		m_PipelineState.Reset();
		m_RootSignature.Reset();
		RebuildPipelineKey();
	}

	void MaterialTemplate::RebuildPipelineKey()
	{
		// Pack discriminating fields into a 64-bit key.
		// Bits [0..7]  : BlendPolicy
		// Bits [8..15] : PassTarget
		// Bits [16..23]: CullMode
		// Bits [24..31]: FillMode
		// Bits [32..47]: DepthBias (clamped to 16 bits)
		// Bits [48..63]: shader name hash (VS ^ PS)
		uint64_t shaderHash =
			std::hash<std::string>{}(m_VertexShaderName) ^
			(std::hash<std::string>{}(m_PixelShaderName) << 1);

		m_PipelineKey =
			(static_cast<uint64_t>(m_BlendPolicy) & 0xFF) |
			(static_cast<uint64_t>(m_PassTarget) << 8 & 0xFF00) |
			(static_cast<uint64_t>(m_RasterizerPolicy.CullMode) << 16 & 0xFF0000) |
			(static_cast<uint64_t>(m_RasterizerPolicy.FillMode) << 24 & 0xFF000000) |
			((static_cast<uint64_t>(m_RasterizerPolicy.DepthBias) << 32) & 0xFFFF00000000ULL) |
			(shaderHash & 0xFFFF000000000000ULL);
	}
}
