#pragma once
#include "RenderWindow.h"
#include "Queues/CommandQueueManager.h"
#include "Heaps/DescriptorHeapManager.h"
#include "../Resources/Shader.h"
#include "../Rendering/GPUUploader.h"
#include "../Application.h"
#include "RenderPass/RenderPassData.h"

namespace DX12Engine
{
	class PipelineStateCache;
	class RootSignatureCache;
	class Camera;
	class ConstantBuffer;

	class RenderContext
	{
	public:
		RenderContext(Application* app, int width, int height, std::string name);
		~RenderContext();

		DirectX::XMINT2								GetWindowSize() const { return m_WindowSize; }
		DirectX::XMINT2								GetRenderSize() const { return m_RenderSize; }
		float										GetRenderScale() const { return m_RenderScale; }
		HWND										GetWindowHandle() const { return m_RenderWindow->GetWindowHandle(); }
		Microsoft::WRL::ComPtr<ID3D12Device>		GetDevice() const { return m_Device; }
		CD3DX12_CPU_DESCRIPTOR_HANDLE				GetRTVHandle() const { return m_RenderWindow->GetRTVHandle(); }
		D3D12_CPU_DESCRIPTOR_HANDLE					GetDSVHandle() const { return m_RenderWindow->GetDSVHandle(); }
		CommandQueueManager&						GetQueueManager() const { return *m_QueueManager; }
		DescriptorHeapManager&						GetHeapManager() const { return *m_HeapManager; }
		GPUUploader&								GetUploader() const { return *m_Uploader; }

		void						UpdateWindowSize(DirectX::XMINT2 windowSize);
		void						UpdateRenderScale(float renderScale);
		CD3DX12_RESOURCE_BARRIER	TransitionRenderTarget(bool forward) const { return m_RenderWindow->TransitionRenderTarget(forward); }
		bool						ProcessWindowMessages() const { return m_RenderWindow->ProcessWindowMessages(); }
		void						PresentFrame() const { m_RenderWindow->PresentFrame(); }

		void UpdateScreenData(Camera* camera, const DirectX::XMFLOAT2& jitter, const DirectX::XMFLOAT2& prevJitter, const DirectX::XMMATRIX* projectionOverride = nullptr);
		ConstantBuffer& GetScreenDataBuffer() const { return *m_ScreenDataCB; }
		ScreenData GetScreenData() const { return m_ScreenData; }
		bool IsPendingResize() const { return m_PendingResize; }

		void OnResize();
		DirectX::XMINT2 GetPendingWindowSize() const { return m_PendingWindowSize; }

	private:
		void InitDevice(HWND hwnd);

		std::unique_ptr<RenderWindow> m_RenderWindow;
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		std::unique_ptr<CommandQueueManager> m_QueueManager;
		std::unique_ptr<DescriptorHeapManager> m_HeapManager;
		std::unique_ptr<GPUUploader> m_Uploader;

		DirectX::XMINT2 m_WindowSize;
		DirectX::XMINT2 m_PendingWindowSize;
		DirectX::XMINT2 m_RenderSize;
		bool m_PendingResize = false;
		float m_RenderScale = 1.0f;

		ScreenData m_ScreenData;
		std::unique_ptr<ConstantBuffer> m_ScreenDataCB;
	};
}

