#pragma once
#include <cstdint>
#include <wtypes.h>
#include <vector>

namespace DX12Engine
{
	class UIConfig;
	class UIFrameContext;
	class UIRenderContext;

	enum class BackendType
	{
		Debug,
		Runtime,
	};

	class IUIBackend
	{
	public:
		virtual ~IUIBackend() = default;

		virtual const char* GetName() const = 0;
		virtual BackendType GetType() const = 0;

		virtual bool Initialize(const UIConfig& config) = 0;
		virtual void Shutdown() = 0;

		virtual void BeginFrame(const UIFrameContext& context) = 0;
		virtual bool HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) = 0;
		virtual void Render(const UIRenderContext& context) = 0;
		virtual void EndFrame() = 0;

		virtual void OnResize(uint32_t width, uint32_t height) = 0;

		virtual bool WantsKeyboardCapture() const = 0;
		virtual bool WantsMouseCapture() const = 0;
		virtual bool IsEnabled() const = 0;
	};
}