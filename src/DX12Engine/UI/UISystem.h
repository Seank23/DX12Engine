#pragma once
#include "IUIBackend.h"
#include <wtypes.h>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>

namespace Rml
{
	class ElementDocument;
}

namespace DX12Engine
{
	struct UIInputState
	{
		bool IsMouseDown[3] = { false, false, false }; // Left, Right, Middle
		POINT MousePosition = { 0, 0 };
		POINT MouseDelta = { 0, 0 };
		bool IsKeyboardDown[256] = { false }; // Virtual-Key codes
	};

	class UISystem
	{
	public:
		UISystem() = default;
		~UISystem() = default;

		bool Initialize(const UIConfig& config);
		void Shutdown();

		void BeginFrame(const UIFrameContext& context);
		bool HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		void Render(const UIRenderContext& context);
		void EndFrame();

		void UpdateWindowSize(uint32_t width, uint32_t height);

		bool WantsKeyboardCapture() const;
		bool WantsMouseCapture() const;
		bool IsInitialized() const;

		bool ShowRuntimeDocument(const std::string& documentPath);
		bool HideRuntimeDocument(const std::string& documentPathOrId);
		Rml::ElementDocument* GetRuntimeDocument(const std::string& documentPathOrId) const;
		bool DispatchRuntimeEvent(const std::string& documentPathOrId, const std::string& eventName);

	private:
		void OnResize(uint32_t width, uint32_t height);

		std::vector<std::unique_ptr<IUIBackend>> m_Backends;
		UIInputState m_InputState;
		bool m_Initialized = false;
		bool m_RuntimeUiEnabled = false;
		bool m_DebugUiEnabled = false;

		uint32_t m_PendingWidth = 0;
		uint32_t m_PendingHeight = 0;
		bool m_PendingResize = false;
	};
}
