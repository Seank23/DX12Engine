#pragma once
#include "../IUIBackend.h"
#include <RmlUi/Core.h>

#include "RmlRenderInterfaceDX12.h"
#include "RmlSystemInterface.h"

#include <filesystem>
#include <cstdint>
#include <wtypes.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace DX12Engine
{
	class RmlUIBackend : public IUIBackend
	{
	public:
		RmlUIBackend();
		~RmlUIBackend();

		virtual const char* GetName() const override { return "RmlUIBackend"; }
		virtual BackendType GetType() const override { return BackendType::Runtime; }

		virtual bool Initialize(const UIConfig& config) override;
		virtual void Shutdown() override;

		virtual void BeginFrame(const UIFrameContext& context) override;
		virtual bool HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
		virtual void Render(const UIRenderContext& context) override;
		virtual void EndFrame() override;

		virtual void OnResize(uint32_t width, uint32_t height) override;

		virtual bool WantsKeyboardCapture() const override;
		virtual bool WantsMouseCapture() const override;
		virtual bool IsEnabled() const override;

		bool ShowDocument(const std::string& documentPath);
		bool HideDocument(const std::string& documentPathOrId);
		Rml::ElementDocument* GetDocument(const std::string& documentPathOrId) const;
		bool DispatchEvent(const std::string& documentPathOrId, const std::string& eventName);

	private:
		static std::string NormalizeDocumentKey(const std::string& key);
		void RegisterDocumentPath(const std::string& documentPath, Rml::ElementDocument* document);

		Rml::Context* m_Context = nullptr;
		std::unique_ptr<RmlSystemInterfaceWin32> m_SystemInterface;
		std::unique_ptr<RmlFileInterface> m_FileInterface;
		std::unique_ptr<RmlRenderInterfaceDX12> m_RenderInterface;

		std::filesystem::path m_UIRootPath = "res/UI";
		Rml::String m_ContextName = "DX12EngineRuntimeUI";

		bool m_IsInitialized = false;
		bool m_RmlInitialized = false;
		bool m_WantsKeyboardCapture = false;
		bool m_WantsMouseCapture = false;
		bool m_MouseCapturedByUi = false;
		uint8_t m_UiCapturedMouseButtonMask = 0;
		std::unordered_map<std::string, Rml::ElementDocument*> m_DocumentMap;
	};
}
