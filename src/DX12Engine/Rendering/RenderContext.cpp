#include "RenderContext.h"
#include "../Resources/ResourceManager.h"
#include "./PipelineStateCache.h"
#include "./RootSignatureCache.h"
#include "../Input/Camera.h"
#include "../Utils/EngineUtils.h"

#include <algorithm>
#include <cmath>

namespace DX12Engine
{
	RenderContext::RenderContext(Application* app, int width, int height, std::string name)
		: m_WindowSize(DirectX::XMINT2(width, height)), m_RenderSize(DirectX::XMINT2(width, height)), m_Device(nullptr)
	{
		m_RenderWindow = std::make_unique<RenderWindow>();
		HWND windowHandle = m_RenderWindow->Init(app, m_WindowSize, name);

		InitDevice(windowHandle);

		m_QueueManager = std::make_unique<CommandQueueManager>(m_Device.Get());
		m_HeapManager = std::make_unique<DescriptorHeapManager>(m_Device);
		m_Uploader = std::make_unique<GPUUploader>(*this);

		ResourceManager::GetInstance().Init(*this);

		m_RenderWindow->CreateSwapChain(m_QueueManager->GetGraphicsQueue().GetCommandQueue().Get());
		m_RenderWindow->CreateRTVHeap(m_Device.Get());
		m_RenderWindow->CreateDepthStencilBuffer();

		m_ScreenDataCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(ScreenData));
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2(static_cast<float>(m_RenderSize.x), static_cast<float>(m_RenderSize.y));
	}

	RenderContext::~RenderContext()
	{
		ResourceManager::Shutdown();
		m_QueueManager.reset();
		m_Device.Reset();
		m_RenderWindow.reset();
		m_Uploader.reset();
	}

	void RenderContext::InitDevice(HWND hwnd)
	{
#if defined(_DEBUG)
		Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			debugController->EnableDebugLayer();
#endif

		HRESULT deviceResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
		if (FAILED(deviceResult))
		{
			MessageBox(hwnd, "Failed to create DirectX 12 device.", "Error", MB_OK);
			exit(-1);
		}
	}

	void RenderContext::UpdateWindowSize(DirectX::XMINT2 windowSize)
	{
		m_PendingWindowSize = windowSize;
		m_PendingResize = true;
	}

	void RenderContext::UpdateRenderScale(float renderScale)
	{
		if (renderScale == m_RenderScale || renderScale < 0.5f)
			return;
		m_RenderScale = renderScale;
		m_PendingWindowSize = m_WindowSize;
		m_PendingResize = true;
	}

	void RenderContext::OnResize()
	{
		m_QueueManager->WaitForAllIdle();
		auto commandQueue = m_QueueManager->GetGraphicsQueue().GetCommandQueue().Get();
		// Ensure all prior queue work (including present-related work) has completed
		// before releasing old back buffers and resizing the swap chain.
		Microsoft::WRL::ComPtr<ID3D12Fence> resizeFence;
		EngineUtils::ThrowIfFailed(m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&resizeFence)));
		const UINT64 resizeFenceValue = 1;
		HANDLE resizeFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		EngineUtils::Assert(resizeFenceEvent != nullptr);
		EngineUtils::ThrowIfFailed(commandQueue->Signal(resizeFence.Get(), resizeFenceValue));
		if (resizeFence->GetCompletedValue() < resizeFenceValue)
		{
			EngineUtils::ThrowIfFailed(resizeFence->SetEventOnCompletion(resizeFenceValue, resizeFenceEvent));
			WaitForSingleObject(resizeFenceEvent, INFINITE);
		}
		CloseHandle(resizeFenceEvent);

		m_WindowSize = m_PendingWindowSize;
		m_RenderWindow->OnResize(m_WindowSize, m_Device.Get());

		m_RenderSize.x = (std::max)(1, static_cast<int>(std::lround(static_cast<double>(m_WindowSize.x) * m_RenderScale)));
		m_RenderSize.y = (std::max)(1, static_cast<int>(std::lround(static_cast<double>(m_WindowSize.y) * m_RenderScale)));
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2(static_cast<float>(m_RenderSize.x), static_cast<float>(m_RenderSize.y));
		m_PendingResize = false;
	}

	void RenderContext::UpdateScreenData(Camera* camera, const DirectX::XMFLOAT2& jitter, const DirectX::XMFLOAT2& prevJitter, const DirectX::XMMATRIX* projectionOverride)
	{
		if (camera != nullptr)
		{
			m_ScreenData.ScreenSize = DirectX::XMFLOAT2(static_cast<float>(m_RenderSize.x), static_cast<float>(m_RenderSize.y));
			m_ScreenData.CameraPosition = DirectX::XMFLOAT4(camera->GetPosition().x, camera->GetPosition().y, camera->GetPosition().z, 1.0f);
			m_ScreenData.ViewMatrix = camera->GetViewMatrix();
			m_ScreenData.ProjectionMatrix = projectionOverride ? *projectionOverride : camera->GetProjectionMatrix();
			m_ScreenData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, camera->GetViewMatrix());
			m_ScreenData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, m_ScreenData.ProjectionMatrix);
			m_ScreenData.Jitter = jitter;
			m_ScreenData.PrevJitter = prevJitter;
			m_ScreenDataCB->Update(&m_ScreenData, sizeof(ScreenData));
		}
	}
}
