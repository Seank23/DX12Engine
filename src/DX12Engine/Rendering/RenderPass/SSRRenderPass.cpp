#include "SSRRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../Buffers/ConstantBuffer.h"
#include "../../Input/Camera.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	SSRRenderPass::SSRRenderPass(RenderContext& context)
		: RenderPass(context)
	{
	}

	SSRRenderPass::~SSRRenderPass()
	{
	}

	void SSRRenderPass::Init()
	{
		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R8G8B8A8_UNORM));

		ResourceManager::GetInstance().UpdateSRVDescriptors(EngineUtils::VectorSharedPtrToPtrs(m_InputResources));
		ResourceManager::GetInstance().UpdateSRVDescriptors(reinterpret_cast<std::vector<GPUResource*> const&>(m_RenderTargets));
		AddDescriptorTableConfig({ (UINT)m_InputResources.size(), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 });

		m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		m_SSRPassCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(SSRPassData));
		m_SSRPassData.ScreenSize = DirectX::XMFLOAT2(windowSize.x, windowSize.y);

		CreateSSRPassPSO();
	}

	void SSRRenderPass::Execute()
	{
		UpdateSSRPassCB();
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

		m_CommandList.SetGraphicsRootConstantBufferView(0, m_SSRPassCB->GetGPUAddress());
		int startIndex = 1;
		for (int i = 0; i < m_DescriptorTableConfigs.size(); i++)
		{
			int resourceIndex = m_DescriptorTableConfigs[i].BaseShaderRegister;
			m_CommandList.SetGraphicsRootDescriptorTable(startIndex + i, m_InputResources[resourceIndex]->GetDescriptor()->GetGPUHandle());
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
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	RenderTexture* SSRRenderPass::GetRenderTarget(RenderTargetType type)
	{
		switch (type)
		{
		case RenderTargetType::Composite:
			return m_RenderTargets[0].get();
		default:
			return nullptr;
		}
	}

	void SSRRenderPass::CreateSSRPassPSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0).SetVertexShader(ResourceManager::GetInstance().GetShader("RenderTriangle_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("SSRPass_PS"));

		rootSignatureBuilder = rootSignatureBuilder.AddConstantBuffer(0)
			.AddDescriptorTables(m_DescriptorTableConfigs)
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}

	void SSRRenderPass::UpdateSSRPassCB()
	{
		m_SSRPassData.CameraPosition = DirectX::XMFLOAT4(m_Camera->GetPosition().x, m_Camera->GetPosition().y, m_Camera->GetPosition().z, 1.0f);
		m_SSRPassData.ViewMatrix = m_Camera->GetViewMatrix();
		m_SSRPassData.ProjectionMatrix = m_Camera->GetProjectionMatrix();
		m_SSRPassData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, m_Camera->GetViewMatrix());
		m_SSRPassData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, m_Camera->GetProjectionMatrix());
		m_SSRPassCB->Update(&m_SSRPassData, sizeof(SSRPassData));
	}
}
