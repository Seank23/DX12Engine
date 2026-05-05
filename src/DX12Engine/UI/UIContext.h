#pragma once
#include <wtypes.h>
#include <cstdint>
#include "d3dx12.h"

namespace DX12Engine
{
	class RenderContext;

	struct UIConfig
	{
		RenderContext* EngineRenderContext = nullptr;
		HWND WindowHandle = nullptr;
		bool EnableRuntimeUI = false;
		bool EnableDebugUI = false;
		uint32_t InitialWidth = 0;
		uint32_t InitialHeight = 0;
	};

	struct UIRenderContext
	{
		ID3D12GraphicsCommandList* CommandList = nullptr;
		ID3D12Device* Device = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView;
		D3D12_VIEWPORT Viewport;
		D3D12_RECT ScissorRect;
		DXGI_FORMAT RenderTargetFormat;
	};

	struct UIFrameContext
	{
		float DeltaSeconds = 0.0f;
		float ElapsedSeconds = 0.0f;
		uint64_t FrameIndex = 0;
		uint32_t ScreenWidth = 0;
		uint32_t ScreenHeight = 0;
	};
}