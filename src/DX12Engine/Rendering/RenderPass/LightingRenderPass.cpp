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
		// If no external textures (skybox cubemaps) were provided, insert black fallback
		// cubemaps so t0 (environmentMap) and t1 (irradianceMap) always have valid descriptors.
		if (!m_HasSkybox)
		{
			m_FallbackEnvMap = ResourceManager::GetInstance().CreateDefaultCubeMap();
			m_FallbackIrradianceMap = ResourceManager::GetInstance().CreateDefaultCubeMap();
		m_InputResources.insert(m_InputResources.begin(),
			{ std::shared_ptr<GPUResource>(m_FallbackEnvMap.get(), [](GPUResource*){}),
			  std::shared_ptr<GPUResource>(m_FallbackIrradianceMap.get(), [](GPUResource*){})
			});
			m_ResourceBlockSizes.insert(m_ResourceBlockSizes.begin(), 2);
		}

		RenderPass::Init();


		m_VertexShaderName = m_VertexShaderName.empty() ? "RenderTriangle_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "PBRLightingDeferred_PS" : m_PixelShaderName;

		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R8G8B8A8_UNORM));
		ResourceManager::GetInstance().UpdateSRVDescriptors(reinterpret_cast<std::vector<GPUResource*> const&>(m_RenderTargets));

		m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		CreateLightingPassPSO();
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

		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_CommandList.ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList.SetDescriptorHeaps(1, &srvHeap);

		m_CommandList.SetGraphicsRootConstantBufferView(0, m_ScreenDataCB->GetGPUAddress());
		m_CommandList.SetGraphicsRootConstantBufferView(1, m_LightBuffer->GetCBVAddress());
		int startIndex = 2;
		for (int i = 0; i < m_InputResourceBlockHandles.size(); i++)
		{
			m_CommandList.SetGraphicsRootDescriptorTable(startIndex + i, m_InputResourceBlockHandles[i].GetGPUHandle());
		}

		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList.DrawInstanced(3, 1, 0, 0);

		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		m_CommandList.ResourceBarrier(1, &barrier);
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(fenceVal);
	}

	RenderTexture* LightingRenderPass::GetRenderTarget(RenderTargetType type)
	{
		switch (type)
		{
		case RenderTargetType::Composite:
			return m_RenderTargets[0].get();
		default:
			return nullptr;
		}
	}

	void LightingRenderPass::CreateLightingPassPSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0).SetVertexShader(ResourceManager::GetInstance().GetShader(m_VertexShaderName))
			.SetPixelShader(ResourceManager::GetInstance().GetShader(m_PixelShaderName));

		rootSignatureBuilder = rootSignatureBuilder.AddConstantBuffer(0).AddConstantBuffer(1)
			.AddDescriptorTables(m_DescriptorTableConfigs)
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC)
			.AddShadowMapSampler(1);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
