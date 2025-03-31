#include "Renderer.h"
#include "../Heaps/DescriptorHeapHandle.h"
#include "../Heaps/DescriptorHeapManager.h"

namespace DX12Engine
{
	Renderer::Renderer(std::shared_ptr<RenderContext> context)
		: m_RenderContext(context), m_RenderHeap(context->GetHeapManager().GetRenderPassHeap()), m_QueueManager(context->GetQueueManager())
	{
		m_CommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	{
		if (!m_RenderContext->GetUploader().UploadAllPending()) // Upload any pending resources
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		m_CommandList->RSSetViewports(1, &viewport);
		m_CommandList->RSSetScissorRects(1, &scissorRect);

		auto barrier = m_RenderContext->TransitionRenderTarget(true);
		m_CommandList->ResourceBarrier(1, &barrier);

		auto rtvHandle = m_RenderContext->GetRTVHandle();
		auto dsvHandle = m_RenderContext->GetDSVHandle();
		m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto srvHeap = m_RenderHeap.GetHeap();
		m_CommandList->SetDescriptorHeaps(1, &srvHeap);

		if (m_Skybox != nullptr) RenderSkybox();
	}

	void Renderer::Render(RenderObject* renderObject)
	{
		// Object binding
		renderObject->UpdateConstantBufferData(m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix(), m_Camera->GetPosition());
		m_CommandList->SetGraphicsRootSignature(renderObject->m_Material->GetRootSignature().Get());
		m_CommandList->SetGraphicsRootConstantBufferView(0, m_LightBuffer->GetCBVAddress());
		m_CommandList->SetGraphicsRootConstantBufferView(1, renderObject->GetCBVAddress());
		// Material binding
		int startIndex = 2;
		if (renderObject != m_Skybox)
		{
			if (m_Skybox != nullptr)
			{
				D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle = m_Skybox->m_Material->GetTexture()->GetGPUHandle();
				renderObject->m_Material->SetEnvironmentMapHandle(&skyboxHandle);
			}
			if (m_ShadowMap != nullptr)
			{
				D3D12_GPU_DESCRIPTOR_HANDLE shadowMapHandle = m_ShadowMap->GetDescriptor()->GetGPUHandle();
				renderObject->m_Material->SetShadowMapHandle(&shadowMapHandle);
			}
			if (m_ShadowCubeMap != nullptr)
			{
				D3D12_GPU_DESCRIPTOR_HANDLE shadowCubeMapHandle = m_ShadowCubeMap->GetDescriptor()->GetGPUHandle();
				renderObject->m_Material->SetShadowCubeMapHandle(&shadowCubeMapHandle);
			}
		}
		renderObject->m_Material->Bind(m_CommandList, &startIndex);
		// Mesh binding
		auto vertexBufferView = renderObject->m_VertexBuffer->GetVertexBufferView();
		auto indexBufferView = renderObject->m_IndexBuffer->GetIndexBufferView();
		m_CommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		m_CommandList->IASetIndexBuffer(&indexBufferView);
		// Draw
		m_CommandList->DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
	}

	void Renderer::PresentFrame()
	{
		auto barrier = m_RenderContext->TransitionRenderTarget(false);
		m_CommandList->ResourceBarrier(1, &barrier);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();

		m_RenderContext->PresentFrame();
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	bool Renderer::PollWindow()
	{
		return m_RenderContext->ProcessWindowMessages();
	}

	D3D12_VIEWPORT Renderer::GetDefaultViewport()
	{
		DirectX::XMFLOAT2 windowSize = m_RenderContext->GetWindowSize();
		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(windowSize.x);
		viewport.Height = static_cast<float>(windowSize.y);
		viewport.MinDepth = -1.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	D3D12_RECT Renderer::GetDefaultScissorRect()
	{
		DirectX::XMFLOAT2 windowSize = m_RenderContext->GetWindowSize();
		D3D12_RECT scissorRect{};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = static_cast<LONG>(windowSize.x);
		scissorRect.bottom = static_cast<LONG>(windowSize.y);
		return scissorRect;
	}

	void Renderer::RenderSkybox()
	{
		Render(m_Skybox);
	}
}
