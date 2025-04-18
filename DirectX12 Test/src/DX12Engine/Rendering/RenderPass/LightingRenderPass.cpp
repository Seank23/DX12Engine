#include "LightingRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../../Buffers/LightBuffer.h"

namespace DX12Engine
{
	LightingRenderPass::LightingRenderPass(RenderContext& context)
		: RenderPass(context)
	{
	}

	LightingRenderPass::~LightingRenderPass()
	{
	}

	void LightingRenderPass::Init()
	{
		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R8G8B8A8_UNORM));

		m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		CreateLightingPassPSO();
	}

	void LightingRenderPass::Execute()
	{
		RenderTexture* renderTarget = m_RenderTargets[0].get();

		if (!m_RenderContext.GetUploader().UploadAllPending()) // Upload any pending resources
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

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

		m_CommandList.SetGraphicsRootConstantBufferView(0, m_LightBuffer->GetCBVAddress());
		m_CommandList.SetGraphicsRootDescriptorTable(1, m_InputResources[0]->GetDescriptor()->GetGPUHandle());

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
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	RenderTexture* LightingRenderPass::GetRenderTarget(RenderTargetType type)
	{
		return nullptr;
	}

	void LightingRenderPass::CreateLightingPassPSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0).SetVertexShader(ResourceManager::GetInstance().GetShader("PBRLightingDeferred_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("PBRLightingDeferred_PS"));

		DescriptorTableConfig materialTable(5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		DescriptorTableConfig envTable(2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5);
		DescriptorTableConfig shadowTable(2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7);
		rootSignatureBuilder = rootSignatureBuilder.AddConstantBuffer(0)
			.AddDescriptorTables({ materialTable, envTable, shadowTable })
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC)
			.AddShadowMapSampler(1);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
