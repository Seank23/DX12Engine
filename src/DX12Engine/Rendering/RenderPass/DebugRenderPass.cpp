#include "DebugRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../../Utils/EngineUtils.h"


namespace DX12Engine
{
	DX12Engine::DebugRenderPass::DebugRenderPass(RenderContext& context)
		: RenderPass(context)
	{
	}

	DebugRenderPass::~DebugRenderPass()
	{
	}

	void DebugRenderPass::Init()
	{
		DirectX::XMINT3 renderSize{m_RenderContext.GetRenderSize().x, m_RenderContext.GetRenderSize().y, 1};
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(RenderTextureConfig{ renderSize, DXGI_FORMAT_R8G8B8A8_UNORM }));

		ResourceManager::GetInstance().UpdateSRVDescriptors(EngineUtils::VectorSharedPtrToPtrs(m_InputResources));
		ResourceManager::GetInstance().UpdateSRVDescriptors(reinterpret_cast<std::vector<GPUResource*> const&>(m_RenderTargets));
		AddDescriptorTableConfig({ (UINT)m_InputResources.size(), D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0 });

		m_Viewport = { 0.0f, 0.0f, (float)renderSize.x, (float)renderSize.y, -1.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)renderSize.x, (LONG)renderSize.y };

		CreatePSO();
	}

	void DebugRenderPass::Execute()
	{
		RenderTexture* renderTarget = m_RenderTargets[0].get();

		m_RenderContext.GetUploader().UploadAllPending(); // Upload any pending resources
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

		int startIndex = 0;
		/*for (int i = 0; i < m_DescriptorTableConfigs.size(); i++)
		{
			int resourceIndex = m_DescriptorTableConfigs[i].BaseShaderRegister;
			m_CommandList.SetGraphicsRootDescriptorTable(startIndex + i, m_InputResources[resourceIndex]->GetTransientDescriptor()->GetGPUHandle());
		}*/

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

	std::shared_ptr<RenderTexture> DebugRenderPass::GetRenderTarget(ResourceSlot type)
	{
		return nullptr;
	}

	void DebugRenderPass::CreatePSO()
	{
	}
}
