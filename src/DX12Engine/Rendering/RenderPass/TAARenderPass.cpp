#include "TAARenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../Buffers/ConstantBuffer.h"
#include "../../Input/Camera.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	TAARenderPass::TAARenderPass(RenderContext& context)
		: RenderPass(context)
	{
		m_Type = RenderPassType::TAA;
	}

	TAARenderPass::~TAARenderPass()
	{
	}

	void TAARenderPass::Init()
	{
		RenderPass::Init();

		m_VertexShaderName = m_VertexShaderName.empty() ? "RenderTriangle_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "TAAPass_PS" : m_PixelShaderName;

		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();

		// Current-frame composite output
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT));

		// Ping-pong history buffers (R16G16B16A16 to preserve HDR history)
		m_HistoryBuffers[0] = ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_HistoryBuffers[1] = ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT);

		// Temporal constant buffer
		m_TemporalCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(TAATemporalData));
		m_TemporalData.FrameIndex = 0;
		m_TemporalData.PrevViewMatrix = DirectX::XMMatrixIdentity();
		m_TemporalData.PrevProjectionMatrix = DirectX::XMMatrixIdentity();

		m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		CreateTAAPassPSO();
	}

	void TAARenderPass::Execute()
	{
		// Capture the previous frame's matrices BEFORE RenderPass::Execute() calls
		// UpdateCB(), which overwrites m_ScreenData with the current camera state.
		DirectX::XMMATRIX prevView = m_PrevFrameScreenData.ViewMatrix;
		DirectX::XMMATRIX prevProj = m_PrevFrameScreenData.ProjectionMatrix;

		RenderPass::Execute(); // updates m_ScreenData to the current frame

		m_TemporalData.PrevViewMatrix = prevView;
		m_TemporalData.PrevProjectionMatrix = prevProj;
		m_TemporalData.FrameIndex = m_FrameIndex;
		m_TemporalCB->Update(&m_TemporalData, sizeof(TAATemporalData));

		RenderTexture* renderTarget = m_RenderTargets[0].get();

		// Read buffer = previously written frame, write buffer = current frame output
		int readIndex = 1 - m_WriteIndex;
		RenderTexture* historyRead = m_HistoryBuffers[readIndex].get();
		RenderTexture* historyWrite = m_HistoryBuffers[m_WriteIndex].get();

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

		m_CommandList.RSSetViewports(1, &m_Viewport);
		m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		// Transition resources, skipping any barrier where before == after state
		// (the history read buffer starts life as PIXEL_SHADER_RESOURCE and may
		// still be in that state on the first frame, making the barrier a no-op
		// that the D3D12 debug layer rejects).
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		if (renderTarget->GetUsageState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				renderTarget->GetResource(),
				renderTarget->GetUsageState(),
				D3D12_RESOURCE_STATE_RENDER_TARGET));
			renderTarget->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		if (historyWrite->GetUsageState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				historyWrite->GetResource(),
				historyWrite->GetUsageState(),
				D3D12_RESOURCE_STATE_RENDER_TARGET));
			historyWrite->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		if (historyRead->GetUsageState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				historyRead->GetResource(),
				historyRead->GetUsageState(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			historyRead->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		if (!barriers.empty())
			m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		// Render to both the composite output and the history write buffer simultaneously
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2] = {
			renderTarget->GetTextureDescriptor().GetCPUHandle(),
			historyWrite->GetTextureDescriptor().GetCPUHandle()
		};
		m_CommandList.OMSetRenderTargets(2, rtvHandles, FALSE, nullptr);

		const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		m_CommandList.ClearRenderTargetView(rtvHandles[0], clearColor, 0, nullptr);
		m_CommandList.ClearRenderTargetView(rtvHandles[1], clearColor, 0, nullptr);

		auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList.SetDescriptorHeaps(1, &srvHeap);

		std::vector<GPUResource*> historyVec = { historyRead };
		DescriptorHeapHandle historyBlock = ResourceManager::GetInstance().UpdateSRVDescriptors(historyVec);

		auto bindPassInputTables = [this, historyBlock]()
		{
			m_CommandList.SetGraphicsRootConstantBufferView(0, m_RenderContext.GetScreenDataBuffer().GetGPUAddress());
			m_CommandList.SetGraphicsRootConstantBufferView(1, m_TemporalCB->GetGPUAddress());
			m_CommandList.SetGraphicsRootDescriptorTable(2, m_InputResourceBlockHandles[InputResourceType::SceneColor].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(3, historyBlock.GetGPUHandle());
		};
		bindPassInputTables();

		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList.DrawInstanced(3, 1, 0, 0);

		// Transition composite output and history write → SRV
		std::vector<D3D12_RESOURCE_BARRIER> postBarriers;
		postBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		postBarriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			historyWrite->GetResource(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		m_CommandList.ResourceBarrier(static_cast<UINT>(postBarriers.size()), postBarriers.data());
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		historyWrite->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(fenceVal);

		// Swap ping-pong index and advance frame counter
		m_WriteIndex = 1 - m_WriteIndex;
		m_FrameIndex++;
		m_PrevFrameScreenData = m_RenderContext.GetScreenData();
	}

	std::shared_ptr<RenderTexture> TAARenderPass::GetRenderTarget(ResourceSlot type)
	{
		switch (type)
		{
		case ResourceSlot::Composite:
			return m_RenderTargets[0];
		default:
			return nullptr;
		}
	}

	void TAARenderPass::TransitionHistoryBuffer(D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
	{
		for (int i = 0; i < 2; i++)
		{
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				m_HistoryBuffers[i]->GetResource(), from, to);
			m_CommandList.ResourceBarrier(1, &barrier);
			m_HistoryBuffers[i]->SetUsageState(to);
		}
	}

	void TAARenderPass::CreateTAAPassPSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		// Two render targets: composite output (SV_TARGET0) and history write buffer (SV_TARGET1)
		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT })
			.SetSampleDesc(UINT_MAX, 1, 0)
			.SetVertexShader(ResourceManager::GetInstance().GetShader(m_VertexShaderName))
			.SetPixelShader(ResourceManager::GetInstance().GetShader(m_PixelShaderName));

		// b0 = ScreenData, b1 = TAATemporalData
		// then descriptor tables for G-buffer resources, then one extra table for history read
		// Build all descriptor table configs in a single call to avoid vector reallocation invalidating pointers
		std::vector<DescriptorTableConfig> allTableConfigs = m_DescriptorTableConfigs;
		allTableConfigs.push_back({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, (UINT)m_InputResources.size() });
		rootSignatureBuilder = rootSignatureBuilder
			.AddConstantBuffer(0)
			.AddConstantBuffer(1)
			.AddDescriptorTables(allTableConfigs)
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
