#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

namespace DX12Engine
{
	class RenderWindow
	{
	public:
		RenderWindow();
		~RenderWindow();

		HWND Init(DirectX::XMFLOAT2 windowSize);
		void CreateSwapChain(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue);
		void CreateRTVHeap(Microsoft::WRL::ComPtr<ID3D12Device> device);
		void CreateDepthStencilBuffer(Microsoft::WRL::ComPtr<ID3D12Device> device);

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() 
		{ 
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex, m_RTVDescriptorSize); 
		}

		D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() 
		{
			return m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
		}

		HWND GetWindowHandle() const { return m_WindowHandle; }

		CD3DX12_RESOURCE_BARRIER TransitionRenderTarget(bool forward);
		void PresentFrame();

		bool ProcessWindowMessages();

	private:
		HWND m_WindowHandle;
		HINSTANCE m_WindowInstance;
		DirectX::XMFLOAT2 m_WindowSize;

		Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
		UINT m_FrameIndex = 0;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderTargets[2];
		Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthStencilBuffer;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE m_DSVHandle;

		UINT m_RTVDescriptorSize;
		UINT m_DSVDescriptorSize;
	};
}

