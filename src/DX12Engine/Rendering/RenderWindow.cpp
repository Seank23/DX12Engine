#include "RenderWindow.h"
#include "../Resources/ResourceManager.h"
#include "../Utils/EngineUtils.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DX12Engine::Application* app = reinterpret_cast<DX12Engine::Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (uMsg == WM_CREATE)
	{
		// Store the application pointer in the window
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
		return 0;
	}
	else if (uMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	else
	{
		if (app != nullptr)
			app->HandleWindowEvent(hwnd, uMsg, wParam, lParam);
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

namespace DX12Engine
{
	RenderWindow::RenderWindow()
		: m_WindowHandle(nullptr), m_WindowInstance(nullptr), m_SwapChain(nullptr), m_RTVHeap(nullptr),
		  m_FrameIndex(0), m_RTVDescriptorSize(0), m_WindowSize(DirectX::XMINT2(0, 0))
	{
	}

	RenderWindow::~RenderWindow()
	{
		m_WindowHandle = nullptr;
		m_WindowInstance = nullptr;
		for (auto& rt : m_RenderTargets)
			rt.Reset();
		m_RTVHeap.Reset();
		m_SwapChain.Reset();
		m_FrameIndex = 0;
		m_RTVDescriptorSize = 0;
	}

	HWND RenderWindow::Init(Application* app, DirectX::XMINT2 windowSize, std::string windowName)
	{
		m_WindowSize = windowSize;
		m_WindowName = windowName;
		WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, m_WindowInstance, nullptr, nullptr, nullptr, nullptr, "DX12Window", nullptr };
		RegisterClassEx(&wc);
		m_WindowHandle = CreateWindow(wc.lpszClassName, windowName.c_str(), WS_OVERLAPPEDWINDOW, 100, 100, windowSize.x, windowSize.y, nullptr, nullptr, wc.hInstance, app);
		ShowWindow(m_WindowHandle, SW_SHOWDEFAULT);
		return m_WindowHandle;
	}

	void RenderWindow::CreateSwapChain(ID3D12CommandQueue* commandQueue)
	{
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
		CreateDXGIFactory1(IID_PPV_ARGS(&factory));

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = 2; // Double buffering
		swapChainDesc.BufferDesc.Width = m_WindowSize.x;
		swapChainDesc.BufferDesc.Height = m_WindowSize.y;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = m_WindowHandle;
		swapChainDesc.SampleDesc.Count = 1; // No multisampling
		swapChainDesc.Windowed = TRUE;

		Microsoft::WRL::ComPtr<IDXGISwapChain> tempSwapChain;
		factory->CreateSwapChain(commandQueue, &swapChainDesc, &tempSwapChain);
		tempSwapChain.As(&m_SwapChain);

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void RenderWindow::CreateRTVHeap(ID3D12Device* device)
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = 2; // Number of descriptors (one for each buffer)
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_RTVHeap));
		m_RTVDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTVHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < 2; i++)
		{
			m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&m_RenderTargets[i]));
			device->CreateRenderTargetView(m_RenderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_RTVDescriptorSize); // Move to the next descriptor
		}
	}
	void RenderWindow::CreateDepthStencilBuffer()
	{
		m_DepthBuffer = ResourceManager::GetInstance().CreateDepthMap(
			RenderTextureConfig{ DirectX::XMINT3(m_WindowSize.x, m_WindowSize.y, 1), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, DXGI_FORMAT_D24_UNORM_S8_UINT });
	}

	CD3DX12_RESOURCE_BARRIER RenderWindow::TransitionRenderTarget(bool forward)
	{
		if (forward)
		{
			return CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[m_FrameIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		else
		{
			return CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[m_FrameIndex].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT);
		}
	}

	void RenderWindow::PresentFrame()
	{
		m_SwapChain->Present(1, 0);
		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	bool RenderWindow::ProcessWindowMessages()
	{
		MSG msg = {};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT)
			{
				return false;
			}
		}
		return true;
	}

	void RenderWindow::OnResize(DirectX::XMINT2 newSize, ID3D12Device* device)
	{
		if (!m_SwapChain || !device)
			return;

		if (newSize.x <= 0 || newSize.y <= 0)
			return;

		m_WindowSize = newSize;
		for (auto& rt : m_RenderTargets)
			rt.Reset();
		m_DepthBuffer.reset();

		EngineUtils::ThrowIfFailed(m_SwapChain->ResizeBuffers(0, newSize.x, newSize.y, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
		CreateRTVHeap(device);
		CreateDepthStencilBuffer();
	}
}
