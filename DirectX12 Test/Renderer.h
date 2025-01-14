#pragma once
#include "RenderContext.h"
#include "RenderObject.h"

namespace DX12Engine
{
	class Renderer
	{
	public:
		Renderer(std::shared_ptr<RenderContext> context);
		~Renderer();

		void InitFrame(D3D12_VIEWPORT viewport, D3D12_RECT scissorRect);
		void Render(RenderObject* renderObject);
		void PresentFrame();
		bool PollWindow();
		void UpdateCameraPosition(float x, float y, float z);

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		void UpdateMVPMatrix(RenderObject* renderObject);

		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager* m_QueueManager;
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;

		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;

		DirectX::XMFLOAT3 m_CameraPosition;
	};
}

