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

		HWND Init(int width, int height);
		void CreateSwapChain(Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue);
		void CreateRTVHeap(Microsoft::WRL::ComPtr<ID3D12Device> device);

		CD3DX12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() 
		{ 
			return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_RTVHeap->GetCPUDescriptorHandleForHeapStart(), m_FrameIndex, m_RTVDescriptorSize); 
		}

		HWND GetWindowHandle() const { return m_WindowHandle; }

		CD3DX12_RESOURCE_BARRIER TransitionRenderTarget(bool forward);
		void PresentFrame();

		bool ProcessWindowMessages();

		int GetWindowWidth() const { return m_Width; }
		int GetWindowHeight() const { return m_Height; }

	private:
		HWND m_WindowHandle;
		HINSTANCE m_WindowInstance;
		int m_Width = 0, m_Height = 0;

		Microsoft::WRL::ComPtr<IDXGISwapChain3> m_SwapChain;
		UINT m_FrameIndex = 0;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_RTVHeap;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_RenderTargets[2];
		UINT m_RTVDescriptorSize;
	};
}

