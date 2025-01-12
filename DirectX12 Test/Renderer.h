#pragma once
#include "RenderWindow.h"
#include "RenderDevice.h"
#include "RenderObject.h"

namespace DX12Engine
{
	class Renderer
	{
	public:
		Renderer(int width, int height);
		~Renderer();

		void CreatePipeline(Shader* vertexShader, Shader* pixelShader);
		void InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
		void Render(RenderObject* renderObject);
		void PresentFrame();
		bool PollWindow();
		void UpdateCameraPosition(float x, float y, float z);

		HWND GetWindowHandle() const { return m_RenderWindow->GetWindowHandle(); }
		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return m_RenderDevice->GetDevice(); }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		void UpdateMVPMatrix(RenderObject* renderObject);

		std::unique_ptr<RenderWindow> m_RenderWindow;
		std::unique_ptr<RenderDevice> m_RenderDevice;

		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;

		DirectX::XMFLOAT3 m_CameraPosition;

		ConstantBuffer m_ConstantBufferData;
	};
}

