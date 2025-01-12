#include "Renderer.h"

namespace DX12Engine
{
	Renderer::Renderer(int width, int height)
		: m_CameraPosition({ 0.0f, 0.0f, -3.0f }), m_ConstantBufferData({ DirectX::XMMatrixIdentity() })
	{
		m_ViewMatrix = DirectX::XMMatrixLookAtLH(
			DirectX::XMVectorSet(m_CameraPosition.x, m_CameraPosition.y, m_CameraPosition.z, 1.0f), // Camera position
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f),  // Look-at target
			DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)   // Up direction
		);
		m_ProjectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
			DirectX::XM_PIDIV4,           // Field of view (radians)
			static_cast<float>(width) / height, // Aspect ratio (width / height)
			0.1f,                // Near plane
			100.0f               // Far plane
		);

		m_RenderWindow = std::make_unique<RenderWindow>();
		HWND windowHandle = m_RenderWindow->Init(width, height);

		m_RenderDevice = std::make_unique<RenderDevice>();
		m_RenderDevice->Init(windowHandle);

		m_RenderWindow->CreateSwapChain(m_RenderDevice->GetCommandQueue());
		m_RenderWindow->CreateRTVHeap(m_RenderDevice->GetDevice());
		m_RenderWindow->CreateDepthStencilBuffer(m_RenderDevice->GetDevice());

		m_RenderDevice->InitCommandList(m_CommandList);
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::CreatePipeline(Shader* vertexShader, Shader* pixelShader)
	{
		m_RenderDevice->CreatePipelineState(vertexShader, pixelShader);
	}

	void Renderer::InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect)
	{
		m_RenderDevice->ResetCommandAllocatorAndList(m_CommandList);

		m_CommandList->SetGraphicsRootSignature(m_RenderDevice->GetRootSignature().Get());
		m_CommandList->RSSetViewports(1, &viewport);
		m_CommandList->RSSetScissorRects(1, &scissorRect);

		auto barrier = m_RenderWindow->TransitionRenderTarget(true);
		m_CommandList->ResourceBarrier(1, &barrier);

		auto rtvHandle = m_RenderWindow->GetRTVHandle();
		auto dsvHandle = m_RenderWindow->GetDSVHandle();
		m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		m_CommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_CommandList->SetPipelineState(m_RenderDevice->GetPipelineState().Get());
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
		auto barrier = m_RenderWindow->TransitionRenderTarget(false);
		m_CommandList->ResourceBarrier(1, &barrier);

		m_CommandList->Close();
		m_RenderDevice->ExecuteCommandList(m_CommandList);

		m_RenderWindow->PresentFrame();
		m_RenderDevice->UpdateFence();
	}

	bool Renderer::PollWindow()
	{
		return m_RenderWindow->ProcessWindowMessages();
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
		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(m_RenderWindow->GetWindowWidth());
		viewport.Height = static_cast<float>(m_RenderWindow->GetWindowHeight());
		viewport.MinDepth = -1.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	D3D12_RECT Renderer::GetDefaultScissorRect()
	{
		D3D12_RECT scissorRect{};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = static_cast<LONG>(m_RenderWindow->GetWindowWidth());
		scissorRect.bottom = static_cast<LONG>(m_RenderWindow->GetWindowHeight());
		return scissorRect;
	}

	void Renderer::UpdateMVPMatrix(RenderObject* renderObject)
	{
		renderObject->UpdateConstantBufferData(renderObject->m_ModelMatrix * m_ViewMatrix * m_ProjectionMatrix);
		m_RenderDevice->SetConstantBuffer(renderObject->m_ConstantBufferRes, renderObject->m_ConstantBufferData);
	}
}
