#pragma once
#include "RenderWindow.h"
#include "RenderDevice.h"

namespace DX12Engine
{
	class Renderer
	{
	public:
		Renderer(int width, int height);
		~Renderer();

		void CreatePipeline(Shader* vertexShader, Shader* pixelShader);
		void Render(D3D12_VERTEX_BUFFER_VIEW vertexBufferView, D3D12_INDEX_BUFFER_VIEW indexBufferView, D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
		bool PollWindow();

		HWND GetWindowHandle() const { return m_RenderWindow->GetWindowHandle(); }
		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return m_RenderDevice->GetDevice(); }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		std::unique_ptr<RenderWindow> m_RenderWindow;
		std::unique_ptr<RenderDevice> m_RenderDevice;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

		DirectX::XMMATRIX m_WorldMatrix;
		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;
	};
}

