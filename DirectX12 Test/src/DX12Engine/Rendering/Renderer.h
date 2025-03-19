#pragma once
#include "RenderContext.h"
#include "RenderObject.h"
#include "../Resources/Texture.h"
#include "../Heaps/RenderPassDescriptorHeap.h"
#include "../Buffers/LightBuffer.h"

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
		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }
		DirectX::XMFLOAT3 GetCameraPosition() { return m_CameraPosition; }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList* m_CommandList;

		DirectX::XMMATRIX m_ViewMatrix;
		DirectX::XMMATRIX m_ProjectionMatrix;

		DirectX::XMFLOAT3 m_CameraPosition;

		RenderPassDescriptorHeap& m_RenderHeap;
		LightBuffer* m_LightBuffer;
	};
}

