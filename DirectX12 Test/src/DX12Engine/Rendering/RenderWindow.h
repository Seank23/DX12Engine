#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include "../Application.h"
#include "../Resources/RenderTexture.h"

namespace DX12Engine
{
	class DescriptorHeapManager;

	class RenderWindow
	{
	public:
		RenderWindow();
		~RenderWindow();

		HWND Init(Application* app, DirectX::XMINT2 windowSize);
		void CreateSwapChain(ID3D12CommandQueue* commandQueue);
		void CreateRTVHeap(ID3D12Device* device);
		void CreateDepthStencilBuffer();

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() 
		{ 
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex, m_RTVDescriptorSize); 
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() 
		{
			return m_DepthBuffer->GetTextureDescriptor().GetCPUHandle();
		}

		HWND GetWindowHandle() const { return m_WindowHandle; }

		CD3DX12_RESOURCE_BARRIER TransitionRenderTarget(bool forward);
		void PresentFrame();

		bool ProcessWindowMessages();

	private:
		HWND m_WindowHandle;
		HINSTANCE m_WindowInstance;
		DirectX::XMINT2 m_WindowSize;

		Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderTargets[2];

		std::unique_ptr<RenderTexture> m_DepthBuffer;

		UINT m_FrameIndex = 0;
		UINT m_RTVDescriptorSize;
	};
}

