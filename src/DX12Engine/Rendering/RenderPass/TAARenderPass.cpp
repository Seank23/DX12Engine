#define NOMINMAX
#include "TAARenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../Buffers/ConstantBuffer.h"
#include "../../Input/Camera.h"
#include "../../Utils/EngineUtils.h"

#include <algorithm>
#include <cmath>

namespace DX12Engine
{
	float TAARenderPass::MatrixMaxAbsDelta(const DirectX::XMMATRIX& a, const DirectX::XMMATRIX& b)
	{
		DirectX::XMFLOAT4X4 af{};
		DirectX::XMFLOAT4X4 bf{};
		DirectX::XMStoreFloat4x4(&af, a);
		DirectX::XMStoreFloat4x4(&bf, b);

		const float* pa = &af._11;
		const float* pb = &bf._11;
		float maxDelta = 0.0f;
		for (int i = 0; i < 16; ++i)
		{
			maxDelta = std::max(maxDelta, std::fabs(pa[i] - pb[i]));
		}
		return maxDelta;
	}

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
		DirectX::XMINT2 renderSize = m_RenderContext.GetRenderSize();

		const bool hasReactiveMask = m_ResourceBlocks.find(InputResourceType::ReactiveMask) != m_ResourceBlocks.end();
		if (!hasReactiveMask)
		{
			m_FallbackReactiveMask = ResourceManager::GetInstance().CreateRenderTargetTexture(renderSize, DXGI_FORMAT_R8_UNORM, 1, { 0.0f, 0.0f, 0.0f, 0.0f });
			AddInputResources({ std::shared_ptr<GPUResource>(m_FallbackReactiveMask.get(), [](GPUResource*) {}) });
			AddResourceBlock(InputResourceType::ReactiveMask, 1);
			m_UsingFallbackReactiveMask = true;
		}

		RenderPass::Init();

		m_VertexShaderName = m_VertexShaderName.empty() ? "RenderTriangle_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "TAAPass_PS" : m_PixelShaderName;

		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(renderSize, DXGI_FORMAT_R16G16B16A16_FLOAT));

		m_HistoryBuffers[0] = ResourceManager::GetInstance().CreateRenderTargetTexture(renderSize, DXGI_FORMAT_R16G16B16A16_FLOAT);
		m_HistoryBuffers[1] = ResourceManager::GetInstance().CreateRenderTargetTexture(renderSize, DXGI_FORMAT_R16G16B16A16_FLOAT);

		m_TemporalCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(TAATemporalData));
		m_TemporalData.FrameIndex = 0;
		m_TemporalData.EnableHistoryReset = 1;
		m_TemporalData.BaseBlend = m_Settings.BaseBlend;
		m_TemporalData.MinBlend = m_Settings.MinBlend;
		m_TemporalData.MaxBlend = m_Settings.MaxBlend;
		m_TemporalData.VelocityRejection = m_Settings.VelocityRejection;
		m_TemporalData.DepthRejection = m_Settings.DepthRejection;
		m_TemporalData.ClampGamma = m_Settings.ClampGamma;
		m_TemporalData.Sharpness = m_Settings.Sharpness;
		m_TemporalData.DisocclusionDepthThreshold = m_Settings.DisocclusionDepthThreshold;

		m_PrevFrameScreenData = m_RenderContext.GetScreenData();

		m_Viewport = { 0.0f, 0.0f, (float)renderSize.x, (float)renderSize.y, 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)renderSize.x, (LONG)renderSize.y };

		CreatePSO();
	}

	void TAARenderPass::Execute()
	{
		const ScreenData currentScreenData = m_RenderContext.GetScreenData();
		const float viewDelta = MatrixMaxAbsDelta(currentScreenData.ViewMatrix, m_PrevFrameScreenData.ViewMatrix);
		const float projDelta = MatrixMaxAbsDelta(currentScreenData.ProjectionMatrix, m_PrevFrameScreenData.ProjectionMatrix);
		const float cameraPosDelta = std::fabs(currentScreenData.CameraPosition.x - m_PrevFrameScreenData.CameraPosition.x)
			+ std::fabs(currentScreenData.CameraPosition.y - m_PrevFrameScreenData.CameraPosition.y)
			+ std::fabs(currentScreenData.CameraPosition.z - m_PrevFrameScreenData.CameraPosition.z);
		const bool screenSizeChanged =
			std::fabs(currentScreenData.ScreenSize.x - m_PrevFrameScreenData.ScreenSize.x) > 0.5f ||
			std::fabs(currentScreenData.ScreenSize.y - m_PrevFrameScreenData.ScreenSize.y) > 0.5f;
		const bool cameraCut = (viewDelta > 0.35f) || (projDelta > 0.35f) || (cameraPosDelta > 3.0f);
		const bool historyReset = m_ForceHistoryReset || !m_HistoryValid || cameraCut || screenSizeChanged;

		RenderPass::Execute();

		if (m_UsingFallbackReactiveMask && m_FallbackReactiveMask)
		{
			RenderTexture* fallbackReactiveMask = m_FallbackReactiveMask.get();
			if (fallbackReactiveMask->GetUsageState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
			{
				auto toRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(
					fallbackReactiveMask->GetResource(),
					fallbackReactiveMask->GetUsageState(),
					D3D12_RESOURCE_STATE_RENDER_TARGET);
				m_CommandList.ResourceBarrier(1, &toRenderTarget);
				fallbackReactiveMask->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
			}

			const float clearReactiveMask[] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_CommandList.ClearRenderTargetView(fallbackReactiveMask->GetTextureDescriptor().GetCPUHandle(), clearReactiveMask, 0, nullptr);

			auto toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
				fallbackReactiveMask->GetResource(),
				fallbackReactiveMask->GetUsageState(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_CommandList.ResourceBarrier(1, &toShaderResource);
			fallbackReactiveMask->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		m_TemporalData.FrameIndex = m_FrameIndex;
		m_TemporalData.EnableHistoryReset = historyReset ? 1u : 0u;
		m_TemporalData.BaseBlend = m_Settings.BaseBlend;
		m_TemporalData.MinBlend = m_Settings.MinBlend;
		m_TemporalData.MaxBlend = m_Settings.MaxBlend;
		m_TemporalData.VelocityRejection = m_Settings.VelocityRejection;
		m_TemporalData.DepthRejection = m_Settings.DepthRejection;
		m_TemporalData.ClampGamma = m_Settings.ClampGamma;
		m_TemporalData.Sharpness = m_Settings.Sharpness;
		m_TemporalData.DisocclusionDepthThreshold = m_Settings.DisocclusionDepthThreshold;
		m_TemporalCB->Update(&m_TemporalData, sizeof(TAATemporalData));

		RenderTexture* renderTarget = m_RenderTargets[0].get();

		int readIndex = 1 - m_WriteIndex;
		RenderTexture* historyRead = m_HistoryBuffers[readIndex].get();
		RenderTexture* historyWrite = m_HistoryBuffers[m_WriteIndex].get();

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

		m_CommandList.RSSetViewports(1, &m_Viewport);
		m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

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
				m_CommandList.SetGraphicsRootDescriptorTable(3, m_InputResourceBlockHandles[InputResourceType::Depth].GetGPUHandle());
				m_CommandList.SetGraphicsRootDescriptorTable(4, m_InputResourceBlockHandles[InputResourceType::GBuffer].GetGPUHandle());
				m_CommandList.SetGraphicsRootDescriptorTable(5, m_InputResourceBlockHandles[InputResourceType::ReactiveMask].GetGPUHandle());
				m_CommandList.SetGraphicsRootDescriptorTable(6, historyBlock.GetGPUHandle());
			};
		bindPassInputTables();

		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList.DrawInstanced(3, 1, 0, 0);

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

		m_QueueManager.GetGraphicsQueue().ExecuteCommandList();

		m_WriteIndex = 1 - m_WriteIndex;
		m_FrameIndex++;
		m_HistoryValid = true;
		m_ForceHistoryReset = false;
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

	void TAARenderPass::CreatePSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT })
			.SetSampleDesc(UINT_MAX, 1, 0)
			.SetVertexShader(ResourceManager::GetInstance().GetShader(m_VertexShaderName))
			.SetPixelShader(ResourceManager::GetInstance().GetShader(m_PixelShaderName));

		// b0 = ScreenData, b1 = TAATemporalData
		// then descriptor tables for TAA inputs, then one extra table for history read
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
