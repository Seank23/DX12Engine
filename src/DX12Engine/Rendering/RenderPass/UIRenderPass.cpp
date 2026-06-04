#include "UIRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../../UI/UISystem.h"
#include "../../UI/UIContext.h"


namespace DX12Engine
{
	DX12Engine::UIRenderPass::UIRenderPass(RenderContext& context)
		: RenderPass(context)
	{
		m_Type = RenderPassType::UI;
	}

	UIRenderPass::~UIRenderPass()
	{
	}

	void UIRenderPass::Init()
	{
		RenderPass::Init();

		DirectX::XMINT3 renderSize{m_RenderContext.GetRenderSize().x, m_RenderContext.GetRenderSize().y, 1};
		DXGI_FORMAT targetFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		if (!m_InputResources.empty())
		{
			if (RenderTexture* sceneSource = dynamic_cast<RenderTexture*>(m_InputResources[0].get()))
				targetFormat = sceneSource->GetFormat();
		}
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(RenderTextureConfig{ renderSize, targetFormat }));

		m_Viewport = { 0.0f, 0.0f, (float)renderSize.x, (float)renderSize.y, 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)renderSize.x, (LONG)renderSize.y };
	}

	void UIRenderPass::Execute()
	{
		RenderPass::Execute();
		if (m_RenderTargets.empty())
			return;

		RenderTexture* renderTarget = m_RenderTargets[0].get();
		RenderTexture* sceneSource = nullptr;
		if (!m_InputResources.empty())
			sceneSource = dynamic_cast<RenderTexture*>(m_InputResources[0].get());

		m_CommandList.RSSetViewports(1, &m_Viewport);
		m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		bool copiedSceneColor = false;
		if (sceneSource && sceneSource->GetResource()->GetDesc().Format == renderTarget->GetResource()->GetDesc().Format)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				sceneSource->GetResource(),
				sceneSource->GetUsageState(),
				D3D12_RESOURCE_STATE_COPY_SOURCE));
			sceneSource->SetUsageState(D3D12_RESOURCE_STATE_COPY_SOURCE);
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				renderTarget->GetResource(),
				renderTarget->GetUsageState(),
				D3D12_RESOURCE_STATE_COPY_DEST));
			renderTarget->SetUsageState(D3D12_RESOURCE_STATE_COPY_DEST);
			m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			m_CommandList.CopyResource(renderTarget->GetResource(), sceneSource->GetResource());
			copiedSceneColor = true;

			barriers.clear();
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				sceneSource->GetResource(),
				sceneSource->GetUsageState(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			sceneSource->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget->GetTextureDescriptor().GetCPUHandle();
		m_CommandList.OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		if (!copiedSceneColor)
			m_CommandList.ClearRenderTargetView(rtvHandle, renderTarget->GetClearColorArray(), 0, nullptr);

		auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList.SetDescriptorHeaps(1, &srvHeap);

		UIRenderContext uiContext{};
		uiContext.CommandList = &m_CommandList;
		uiContext.Device = m_RenderContext.GetDevice().Get();
		uiContext.RenderTargetView = rtvHandle;
		uiContext.RenderTargetFormat = renderTarget->GetFormat();
		uiContext.Viewport = m_Viewport;
		uiContext.ScissorRect = m_ScissorRect;
		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		uiContext.LogicalWidth = static_cast<uint32_t>(windowSize.x);
		uiContext.LogicalHeight = static_cast<uint32_t>(windowSize.y);
		if (m_UISystem)
			m_UISystem->Render(uiContext);

		barriers.clear();
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		));
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
	}

	std::shared_ptr<RenderTexture> UIRenderPass::GetRenderTarget(ResourceSlot type)
	{
		switch (type)
		{
		case ResourceSlot::Composite:
			return m_RenderTargets[0];
		default:
			return nullptr;
		}
	}

	void UIRenderPass::CreatePSO()
	{
	}
}
