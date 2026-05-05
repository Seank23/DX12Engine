#include "UISystem.h"
#include "UIContext.h"

namespace DX12Engine
{
	static std::vector<BackendType> BackendEventHandlingOrder = { BackendType::Debug, BackendType::Runtime };
	static std::vector<BackendType> BackendRenderingOrder = { BackendType::Runtime, BackendType::Debug };

	bool UISystem::Initialize(const UIConfig& config)
	{
		m_RuntimeUiEnabled = config.EnableRuntimeUI;
		m_DebugUiEnabled = config.EnableDebugUI;
		if (m_RuntimeUiEnabled)
		{
			// Initialize runtime UI backend (e.g., ImGui)
			// m_Backends.push_back(std::make_unique<ImGuiBackend>());
			// if (!m_Backends.back()->Initialize(config))
			// {
			// 	m_Backends.pop_back();
			// 	return false;
			// }
		}
		if (m_DebugUiEnabled)
		{
			// Initialize debug UI backend (e.g., ImGui with debug tools)
			// m_Backends.push_back(std::make_unique<ImGuiDebugBackend>());
			// if (!m_Backends.back()->Initialize(config))
			// {
			// 	m_Backends.pop_back();
			// 	return false;
			// }
		}
		m_Initialized = true;
		return true;
	}

	void UISystem::Shutdown()
	{
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->Shutdown();
		m_Backends.clear();
		m_Initialized = false;
	}

	void UISystem::BeginFrame(const UIFrameContext& context)
	{
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->BeginFrame(context);
	}

	bool UISystem::HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		for (BackendType type : BackendEventHandlingOrder)
		{
			for (auto& backend : m_Backends)
			{
				if (backend->GetType() == type && backend->IsEnabled())
					return backend->HandleWindowEvent(hwnd, msg, wParam, lParam);
			}
		}
		return false;
	}

	void UISystem::Render(const UIRenderContext& context)
	{
		for (BackendType type : BackendRenderingOrder)
		{
			for (auto& backend : m_Backends)
			{
				if (backend->GetType() == type && backend->IsEnabled())
					return backend->Render(context);
			}
		}
	}

	void UISystem::EndFrame()
	{
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->EndFrame();
	}

	void UISystem::OnResize(uint32_t width, uint32_t height)
	{
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->OnResize(width, height);
	}

	bool UISystem::WantsKeyboardCapture() const
	{
		bool wantsCapture = false;
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				wantsCapture |= backend->WantsKeyboardCapture();
		return wantsCapture;
	}

	bool UISystem::WantsMouseCapture() const
	{
		bool wantsCapture = false;
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				wantsCapture |= backend->WantsMouseCapture();
		return wantsCapture;
	}

	bool UISystem::IsInitialized() const
	{
		if (!this) return false;
		return m_Initialized;
	}
}