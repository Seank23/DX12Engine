#include "Renderer.h"

namespace DX12Engine
{
	Renderer::Renderer(std::shared_ptr<RenderContext> context)
		: m_CameraPosition({ 0.0f, 0.0f, -3.0f }), m_RenderContext(context)
	{
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 1.0f), // Camera position
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),  // Look-at target
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Up direction
		);
		DirectX::XMFLOAT2 windowSize = context->GetWindowSize();
		m_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XM_PIDIV4,           // Field of view (radians)
			windowSize.x / windowSize.y, // Aspect ratio (width / height)
			0.1f,                // Near plane
			100.0f               // Far plane
		);

		context->InitCommandList(m_CommandList);
	}

	Renderer::~Renderer()
	{
		m_CommandList.Reset();
	}

	void Renderer::InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	{
		m_RenderContext->ResetCommandAllocatorAndList(m_CommandList);

		m_CommandList->SetGraphicsRootSignature(m_RenderContext->GetRootSignature().Get());
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

		m_CommandList->SetPipelineState(m_RenderContext->GetPipelineState().Get());
		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	}

	void Renderer::Render(RenderObject* renderObject)
	{
		auto vertexBufferView = renderObject->m_VertexBuffer.GetVertexBufferView();
		auto indexBufferView = renderObject->m_IndexBuffer.GetIndexBufferView();
		UpdateMVPMatrix(renderObject);
		m_CommandList->SetGraphicsRootConstantBufferView(0, renderObject->m_ConstantBufferRes->GetGPUVirtualAddress());
		m_CommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
		m_CommandList->IASetIndexBuffer(&indexBufferView);
		m_CommandList->DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
	}

	void Renderer::PresentFrame()
	{
		auto barrier = m_RenderContext->TransitionRenderTarget(false);
		m_CommandList->ResourceBarrier(1, &barrier);

		m_CommandList->Close();
		m_RenderContext->ExecuteCommandList(m_CommandList);

		m_RenderContext->PresentFrame();
		m_RenderContext->UpdateFence();
	}

	bool Renderer::PollWindow()
	{
		return m_RenderContext->ProcessWindowMessages();
	}

	void Renderer::UpdateCameraPosition(float x, float y, float z)
	{
		m_CameraPosition.x += x;
		m_CameraPosition.y += y;
		m_CameraPosition.z += z;
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 1.0f), // Camera position
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),  // Look-at target
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Up direction
		);
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

	void Renderer::UpdateMVPMatrix(RenderObject* renderObject)
	{
		renderObject->UpdateConstantBufferData(renderObject->m_ModelMatrix * m_ViewMatrix * m_ProjectionMatrix);
		m_RenderContext->SetConstantBuffer(renderObject->m_ConstantBufferRes, renderObject->m_ConstantBufferData);
	}
}
