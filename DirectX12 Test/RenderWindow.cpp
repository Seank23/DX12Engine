#include "RenderWindow.h"

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

namespace DX12Engine
{
	RenderWindow::RenderWindow()
		: m_WindowHandle(nullptr), m_WindowInstance(nullptr), m_SwapChain(nullptr)
	{
	}

	RenderWindow::~RenderWindow()
	{
		m_WindowHandle = nullptr;
		m_WindowInstance = nullptr;
		m_SwapChain.Reset();
		m_FrameIndex = 0;
	}

	HWND RenderWindow::Init(int width, int height)
	{
		m_Width = width;
		m_Height = height;
		WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0, 0, m_WindowInstance, nullptr, nullptr, nullptr, nullptr, L"DX12Window", nullptr };
		RegisterClassEx(&wc);
		m_WindowHandle = CreateWindow(wc.lpszClassName, L"DirectX 12 Renderer", WS_OVERLAPPEDWINDOW, 100, 100, width, height, nullptr, nullptr, wc.hInstance, nullptr);
		ShowWindow(m_WindowHandle, SW_SHOWDEFAULT);
		return m_WindowHandle;
	}

	void RenderWindow::CreateSwapChain(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue)
	{
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
		CreateDXGIFactory1(IID_PPV_ARGS(&factory));

		DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
		swapChainDesc.BufferCount = 2; // Double buffering
		swapChainDesc.BufferDesc.Width = m_Width;
		swapChainDesc.BufferDesc.Height = m_Height;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.OutputWindow = m_WindowHandle;
		swapChainDesc.SampleDesc.Count = 1; // No multisampling
		swapChainDesc.Windowed = TRUE;

		Microsoft::WRL::ComPtr<IDXGISwapChain> tempSwapChain;
		factory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, &tempSwapChain);
		tempSwapChain.As(&m_SwapChain);

		m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void RenderWindow::CreateRTVHeap(Microsoft::WRL::ComPtr<ID3D12Device> device)
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
	CD3DX12_RESOURCE_BARRIER RenderWindow::TransitionRenderTarget(bool forward)
	{
		if (forward)
		{
			return CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[m_FrameIndex].Get(),
				D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
		}
		else
		{
			return CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[m_FrameIndex].Get(),
				D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT
			);
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
}
