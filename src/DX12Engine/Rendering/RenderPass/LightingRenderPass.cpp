#include "LightingRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../Buffers/LightBuffer.h"

namespace DX12Engine
{
	LightingRenderPass::LightingRenderPass(RenderContext& context)
		: RenderPass(context)
	{
		m_Type = RenderPassType::Lighting;
	}

	LightingRenderPass::~LightingRenderPass()
	{
	}

	void LightingRenderPass::Init()
	{
		const bool hasEnvMap = m_ResourceBlocks.find(InputResourceType::EnvironmentMap) != m_ResourceBlocks.end();
		if (!hasEnvMap)
		{
			m_FallbackEnvMap = ResourceManager::GetInstance().CreateDefaultCubeMap();
			m_FallbackIrradianceMap = ResourceManager::GetInstance().CreateDefaultCubeMap();
			m_InputResources.insert(m_InputResources.begin(),
				{ std::shared_ptr<GPUResource>(m_FallbackEnvMap.get(), [](GPUResource*) {}),
				  std::shared_ptr<GPUResource>(m_FallbackIrradianceMap.get(), [](GPUResource*) {})
				});
			AddResourceBlock(InputResourceType::EnvironmentMap, 2);
		}

		RenderPass::Init();

		m_VertexShaderName = m_VertexShaderName.empty() ? "RenderTriangle_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "PBRLightingDeferred_PS" : m_PixelShaderName;

		DirectX::XMINT3 renderSize{m_RenderContext.GetRenderSize().x, m_RenderContext.GetRenderSize().y, 1};
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(RenderTextureConfig{ renderSize, DXGI_FORMAT_R16G16B16A16_FLOAT }));

		m_Viewport = { 0.0f, 0.0f, (float)renderSize.x, (float)renderSize.y, -1.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)renderSize.x, (LONG)renderSize.y };

		if (!m_FallbackCascadedShadowCB)
		{
			m_FallbackCascadedShadowCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(CascadedShadowData));
			CascadedShadowData noCSMData{};
			noCSMData.Params0 = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
			m_FallbackCascadedShadowCB->Update(&noCSMData, sizeof(CascadedShadowData));
		}

		CreatePSO();
	}

	void LightingRenderPass::Execute()
	{
		RenderPass::Execute();
		RenderTexture* renderTarget = m_RenderTargets[0].get();

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

		m_CommandList.RSSetViewports(1, &m_Viewport);
		m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);
		m_CommandList.ResourceBarrier(1, &barrier);
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget->GetTextureDescriptor().GetCPUHandle();
		m_CommandList.OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		m_CommandList.ClearRenderTargetView(rtvHandle, renderTarget->GetClearColorArray(), 0, nullptr);

		auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList.SetDescriptorHeaps(1, &srvHeap);

		auto bindPassInputTables = [this]()
		{
			ConstantBuffer* cascadedShadowCB = !m_ExternalCBs.empty() ? m_ExternalCBs[0] : m_FallbackCascadedShadowCB.get();
			m_CommandList.SetGraphicsRootConstantBufferView(0, m_RenderContext.GetScreenDataBuffer().GetGPUAddress());
			m_CommandList.SetGraphicsRootConstantBufferView(1, m_LightBuffer->GetCBVAddress());
			m_CommandList.SetGraphicsRootConstantBufferView(2, cascadedShadowCB->GetGPUAddress());
			m_CommandList.SetGraphicsRootDescriptorTable(3, m_InputResourceBlockHandles[InputResourceType::EnvironmentMap].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(4, m_InputResourceBlockHandles[InputResourceType::GBuffer].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(5, m_InputResourceBlockHandles[InputResourceType::ShadowMap].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(6, m_InputResourceBlockHandles[InputResourceType::CubeShadowMap].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(7, m_InputResourceBlockHandles[InputResourceType::CascadedShadowMap].GetGPUHandle());
		};
		bindPassInputTables();

		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList.DrawInstanced(3, 1, 0, 0);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		m_CommandList.ResourceBarrier(1, &barrier);
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
	}

	std::shared_ptr<RenderTexture> LightingRenderPass::GetRenderTarget(ResourceSlot type)
	{
		switch (type)
		{
		case ResourceSlot::Composite:
			return m_RenderTargets[0];
		default:
			return nullptr;
		}
	}

	void LightingRenderPass::CreatePSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R16G16B16A16_FLOAT })
			.SetSampleDesc(UINT_MAX, 1, 0).SetVertexShader(ResourceManager::GetInstance().GetShader(m_VertexShaderName))
			.SetPixelShader(ResourceManager::GetInstance().GetShader(m_PixelShaderName));

		rootSignatureBuilder = rootSignatureBuilder.AddConstantBuffer(0).AddConstantBuffer(1).AddConstantBuffer(2)
			.AddDescriptorTables(m_DescriptorTableConfigs)
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC)
			.AddShadowMapSampler(1);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
