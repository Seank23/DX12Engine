#include "UISystem.h"
#include "UIContext.h"
#include "RuntimeBackend/RmlUIBackend.h"
#if DX12ENGINE_ENABLE_IMGUI_DEBUG_UI
#include "DebugBackend/ImGuiDebugBackend.h"
#endif

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
			m_Backends.push_back(std::make_unique<RmlUIBackend>());
			if (!m_Backends.back()->Initialize(config))
			{
				m_Backends.pop_back();
				return false;
			}
		}
		if (m_DebugUiEnabled)
		{
#if DX12ENGINE_ENABLE_IMGUI_DEBUG_UI
			m_Backends.push_back(std::make_unique<ImGuiDebugBackend>());
			if (!m_Backends.back()->Initialize(config))
			{
				m_Backends.pop_back();
				return false;
			}
#else
			m_DebugUiEnabled = false;
#endif
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
		if (m_PendingResize && m_PendingHeight > 0 && m_PendingWidth > 0)
		{
			OnResize(m_PendingWidth, m_PendingHeight);
			m_PendingResize = false;
		}

		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->BeginFrame(context);
	}

	bool UISystem::HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		bool consumed = false;
		for (BackendType type : BackendEventHandlingOrder)
		{
			for (auto& backend : m_Backends)
			{
				if (backend->GetType() == type && backend->IsEnabled())
				{
					consumed = backend->HandleWindowEvent(hwnd, msg, wParam, lParam) || consumed;
					if (consumed)
						return true;
				}
			}
		}
		return consumed;
	}

	void UISystem::Render(const UIRenderContext& context)
	{
		for (BackendType type : BackendRenderingOrder)
		{
			for (auto& backend : m_Backends)
			{
				if (backend->GetType() == type && backend->IsEnabled())
					backend->Render(context);
			}
		}
	}

	void UISystem::EndFrame()
	{
		for (auto& backend : m_Backends)
			if (backend->IsEnabled())
				backend->EndFrame();
	}

	void UISystem::UpdateWindowSize(uint32_t width, uint32_t height)
	{
		m_PendingWidth = width;
		m_PendingHeight = height;
		m_PendingResize = true;
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
		if (!this)
			return false;
		return m_Initialized;
	}

	bool UISystem::ShowRuntimeDocument(const std::string& documentPath)
	{
		if (!m_Initialized)
			return false;

		for (const auto& backend : m_Backends)
		{
			if (backend->GetType() != BackendType::Runtime || !backend->IsEnabled())
				continue;

			if (RmlUIBackend* rmlBackend = dynamic_cast<RmlUIBackend*>(backend.get()))
				return rmlBackend->ShowDocument(documentPath);
		}

		return false;
	}

	bool UISystem::HideRuntimeDocument(const std::string& documentPathOrId)
	{
		if (!m_Initialized)
			return false;

		for (const auto& backend : m_Backends)
		{
			if (backend->GetType() != BackendType::Runtime || !backend->IsEnabled())
				continue;

			if (RmlUIBackend* rmlBackend = dynamic_cast<RmlUIBackend*>(backend.get()))
				return rmlBackend->HideDocument(documentPathOrId);
		}

		return false;
	}

	Rml::ElementDocument* UISystem::GetRuntimeDocument(const std::string& documentPathOrId) const
	{
		if (!m_Initialized)
			return nullptr;

		for (const auto& backend : m_Backends)
		{
			if (backend->GetType() != BackendType::Runtime || !backend->IsEnabled())
				continue;

			if (const RmlUIBackend* rmlBackend = dynamic_cast<const RmlUIBackend*>(backend.get()))
				return rmlBackend->GetDocument(documentPathOrId);
		}

		return nullptr;
	}

	bool UISystem::DispatchRuntimeEvent(const std::string& documentPathOrId, const std::string& eventName)
	{
		if (!m_Initialized)
			return false;

		for (const auto& backend : m_Backends)
		{
			if (backend->GetType() != BackendType::Runtime || !backend->IsEnabled())
				continue;

			if (RmlUIBackend* rmlBackend = dynamic_cast<RmlUIBackend*>(backend.get()))
				return rmlBackend->DispatchEvent(documentPathOrId, eventName);
		}

		return false;
	}
}
