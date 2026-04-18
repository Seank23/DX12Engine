#include "RenderContext.h"
#include "../Resources/ResourceManager.h"
#include "./PipelineStateCache.h"
#include "./RootSignatureCache.h"
#include "../Input/Camera.h"

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

	void RenderContext::SetWindowSize(DirectX::XMINT2 windowSize)
	{
		m_WindowSize = windowSize;
		UpdateRenderSize();
	}

	void RenderContext::SetRenderScale(float renderScale)
	{
		m_RenderScale = (std::max)(0.5f, renderScale);
		UpdateRenderSize();
	}

	void RenderContext::UpdateRenderSize()
	{
		m_RenderSize.x = (std::max)(1, static_cast<int>(std::lround(static_cast<double>(m_WindowSize.x) * m_RenderScale)));
		m_RenderSize.y = (std::max)(1, static_cast<int>(std::lround(static_cast<double>(m_WindowSize.y) * m_RenderScale)));
		m_ScreenData.ScreenSize = DirectX::XMFLOAT2(static_cast<float>(m_RenderSize.x), static_cast<float>(m_RenderSize.y));
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
