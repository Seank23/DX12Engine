#pragma once
#include "../IUIBackend.h"
#include "../UIContext.h"
#include "../../Rendering/RendererOptions.h"

#include <imgui.h>
#include <wrl/client.h>
#include "d3dx12.h"
#include <filesystem>
#include <cstdint>
#include <wtypes.h>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

namespace DX12Engine
{
	class RendererOptions;

	class ImGuiDebugBackend : public IUIBackend
	{
	public:
		ImGuiDebugBackend();
		~ImGuiDebugBackend();
		virtual const char* GetName() const override { return "ImGuiDebugBackend"; }
		virtual BackendType GetType() const override { return BackendType::Debug; }

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

	private:
		void DrawDebugPanel();
		bool ProcessEngineInput(UINT msg, WPARAM wParam, LPARAM lParam);

		ImGuiContext* m_ImGuiContext = nullptr;
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_ImGuiSrvHeap;

		std::filesystem::path m_UIRootPath = "res/UI";

		bool m_IsInitialized = false;
		bool m_Visible = false;
		bool m_WantsKeyboardCapture = false;
		bool m_WantsMouseCapture = false;
		bool m_MouseCapturedByUi = false;
		uint8_t m_UiCapturedMouseButtonMask = 0;

		bool m_FrameStarted = false;

		const UIFrameContext* m_FrameContext;
		RendererOptions m_LocalRendererOptions;
	};
}
