#include "ImGuiDebugBackend.h"
#include "../../Rendering/RenderContext.h"
#include "../../Rendering/Heaps/DescriptorHeapManager.h"
#include "../../Input/InputController.h"

#include <Windows.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#include <format>
#include <iostream>
#include <type_traits>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace DX12Engine
{
	namespace
	{
		bool IsMouseMessage(UINT msg)
		{
			switch (msg)
			{
			case WM_MOUSEMOVE:
			case WM_MOUSEWHEEL:
			case WM_MOUSEHWHEEL:
			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK:
			case WM_XBUTTONDOWN:
			case WM_XBUTTONUP:
			case WM_XBUTTONDBLCLK:
				return true;
			default:
				return false;
			}
		}

		bool IsKeyboardMessage(UINT msg)
		{
			switch (msg)
			{
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_CHAR:
			case WM_SYSCHAR:
			case WM_DEADCHAR:
			case WM_SYSDEADCHAR:
				return true;
			default:
				return false;
			}
		}

		const char* AntiAliasingModeName(const AntiAliasingMode mode)
		{
			switch (mode)
			{
			case AntiAliasingMode::None:
				return "None";
			case AntiAliasingMode::FXAA:
				return "FXAA";
			case AntiAliasingMode::TAA:
				return "TAA";
			default:
				return "Unknown";
			}
		}

		template <typename T>
		void DrawOptionValue(const char* name, T& value)
		{
			if constexpr (std::is_same_v<T, bool>)
			{
				ImGui::Checkbox(name, &value);
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				ImGui::InputInt(name, &value, 1.0f);
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				ImGui::InputFloat(name, &value, 0.01f);
			}
			else if constexpr (std::is_same_v<T, AntiAliasingMode>)
			{
				const char* aaModes[] = { "None", "FXAA", "TAA" };
				int currentMode = static_cast<int>(value);
				if (ImGui::Combo(name, &currentMode, aaModes, IM_ARRAYSIZE(aaModes)))
					value = static_cast<AntiAliasingMode>(currentMode);
			}
		}

		void DrawTAASettings(TAASettings& settings)
		{
			#define TAA_FIELDS(X) \
				X(float, BaseBlend) \
				X(float, MinBlend) \
				X(float, MaxBlend) \
				X(float, VelocityRejection) \
				X(float, DepthRejection) \
				X(float, ClampGamma) \
				X(float, Sharpness) \
				X(float, DisocclusionDepthThreshold)

			#define DRAW_TAA_FIELD(type, field) DrawOptionValue(#field, settings.field);
			TAA_FIELDS(DRAW_TAA_FIELD)
			#undef DRAW_TAA_FIELD
			#undef TAA_FIELDS
		}

		void DrawCSMSettings(CSMSettings& settings)
		{
			#define CSM_FIELDS(X) \
				X(int, CascadeCount) \
				X(int, ShadowMapSize) \
				X(float, MaxDistance) \
				X(float, SplitLambda) \
				X(float, CascadeBlend) \
				X(float, ConstantBias) \
				X(float, SlopeBias) \
				X(float, NormalBias)

			#define DRAW_CSM_FIELD(type, field) DrawOptionValue(#field, settings.field);
			CSM_FIELDS(DRAW_CSM_FIELD)
			#undef DRAW_CSM_FIELD
			#undef CSM_FIELDS
		}

		void DrawRendererOptions(RendererOptions& options)
		{
			#define RENDERER_SCALAR_FIELDS(X) \
				X(float, RenderScale) \
				X(AntiAliasingMode, AA_Mode) \
				X(bool, EnableGammaCorrection) \
				X(bool, EnableFrustumCulling)

			#define RENDERER_NESTED_FIELDS(X) \
				X(TAA, DrawTAASettings) \
				X(CSM, DrawCSMSettings)

			#define DRAW_RENDERER_SCALAR(type, field) DrawOptionValue(#field, options.field);
			RENDERER_SCALAR_FIELDS(DRAW_RENDERER_SCALAR)
			#undef DRAW_RENDERER_SCALAR
			#undef RENDERER_SCALAR_FIELDS

			#define DRAW_RENDERER_NESTED(field, drawer) \
				if (ImGui::CollapsingHeader(#field, ImGuiTreeNodeFlags_DefaultOpen)) drawer(options.field);
			RENDERER_NESTED_FIELDS(DRAW_RENDERER_NESTED)
			#undef DRAW_RENDERER_NESTED
			#undef RENDERER_NESTED_FIELDS
		}

		UINT GetSrvDescriptorSize(ID3D12Device* device)
		{
			return device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		void ImGui_SrvAlloc(ImGui_ImplDX12_InitInfo* info,
			D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
			D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
		{
			if (!info || !outCpu || !outGpu || !info->Device || !info->SrvDescriptorHeap || !info->UserData)
			{
				if (outCpu)
					outCpu->ptr = 0;
				if (outGpu)
					outGpu->ptr = 0;
				return;
			}

			auto* renderContext = static_cast<RenderContext*>(info->UserData);
			DescriptorHeapHandle persistentHandle;

			try
			{
				persistentHandle = renderContext->GetHeapManager().AllocatePersistentSRV();
			}
			catch (...)
			{
				outCpu->ptr = 0;
				outGpu->ptr = 0;
				return;
			}

			const UINT descriptorSize = GetSrvDescriptorSize(info->Device);
			*outCpu = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			outCpu->ptr += static_cast<SIZE_T>(persistentHandle.GetHeapIndex()) * descriptorSize;

			*outGpu = info->SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
			outGpu->ptr += static_cast<UINT64>(persistentHandle.GetHeapIndex()) * descriptorSize;
		}

		void ImGui_SrvFree(ImGui_ImplDX12_InitInfo* info,
			D3D12_CPU_DESCRIPTOR_HANDLE cpu,
			D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			if (!info || !info->Device || !info->SrvDescriptorHeap || !info->UserData || cpu.ptr == 0)
				return;

			auto* renderContext = static_cast<RenderContext*>(info->UserData);
			DescriptorHeapManager& heapManager = renderContext->GetHeapManager();

			const UINT descriptorSize = GetSrvDescriptorSize(info->Device);
			if (descriptorSize == 0)
				return;

			const D3D12_CPU_DESCRIPTOR_HANDLE heapStart = info->SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			if (cpu.ptr < heapStart.ptr)
				return;

			const SIZE_T byteOffset = cpu.ptr - heapStart.ptr;
			const UINT heapIndex = static_cast<UINT>(byteOffset / descriptorSize);

			DescriptorHeapHandle persistentHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE persistentCpu = heapManager.GetStagingHeap().GetHeapCPUStart();
			persistentCpu.ptr += static_cast<SIZE_T>(heapIndex) * heapManager.GetStagingHeap().GetDescriptorSize();
			persistentHandle.SetCPUHandle(persistentCpu);
			persistentHandle.SetHeapIndex(heapIndex);

			heapManager.ReleasePersistentSRV(persistentHandle);
		}
	}

	ImGuiDebugBackend::ImGuiDebugBackend()
	{
	}

	ImGuiDebugBackend::~ImGuiDebugBackend()
	{
	}

	bool ImGuiDebugBackend::Initialize(const UIConfig& config)
	{
		if (m_IsInitialized)
			return true;

		if (!config.EngineRenderContext || !config.WindowHandle)
			return false;

		if (!IMGUI_CHECKVERSION())
			return false;

		m_ImGuiContext = ImGui::CreateContext();
		if (!m_ImGuiContext)
			return false;

		ImGui::SetCurrentContext(m_ImGuiContext);

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		if (!ImGui_ImplWin32_Init(config.WindowHandle))
		{
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
			return false;
		}

		ID3D12Device* device = config.EngineRenderContext->GetDevice().Get();
		if (!device)
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
			return false;
		}

		D3D12_DESCRIPTOR_HEAP_DESC imguiHeapDesc = {};
		imguiHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		imguiHeapDesc.NumDescriptors = SRV_PERSISTENT_CAPACITY;
		imguiHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		imguiHeapDesc.NodeMask = 0;

		if (FAILED(device->CreateDescriptorHeap(&imguiHeapDesc, IID_PPV_ARGS(&m_ImGuiSrvHeap))))
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
			return false;
		}

		ImGui_ImplDX12_InitInfo init = {};
		init.Device = device;
		init.CommandQueue = config.EngineRenderContext->GetQueueManager().GetGraphicsQueue().GetCommandQueue().Get();
		init.NumFramesInFlight = static_cast<int>(FRAMES_IN_FLIGHT);
		init.RTVFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		init.DSVFormat = DXGI_FORMAT_UNKNOWN;
		init.SrvDescriptorHeap = m_ImGuiSrvHeap.Get();
		init.SrvDescriptorAllocFn = &ImGui_SrvAlloc;
		init.SrvDescriptorFreeFn = &ImGui_SrvFree;
		init.UserData = config.EngineRenderContext;

		if (!ImGui_ImplDX12_Init(&init))
		{
			m_ImGuiSrvHeap.Reset();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
			return false;
		}

		m_IsInitialized = true;
		return true;
	}

	void ImGuiDebugBackend::Shutdown()
	{
		if (!m_IsInitialized)
			return;
		ImGui_ImplDX12_Shutdown();
		m_ImGuiSrvHeap.Reset();
		ImGui_ImplWin32_Shutdown();
		if (m_ImGuiContext)
		{
			ImGui::DestroyContext(m_ImGuiContext);
			m_ImGuiContext = nullptr;
		}
		m_WantsKeyboardCapture = false;
		m_WantsMouseCapture = false;
		m_MouseCapturedByUi = false;
		m_UiCapturedMouseButtonMask = 0;
		m_IsInitialized = false;
	}

	void ImGuiDebugBackend::BeginFrame(const UIFrameContext& context)
	{
		if (!m_Visible) return;

		m_FrameStarted = true;
		m_FrameContext = &context;
		m_LocalRendererOptions = *context.DebugSnapshot.RendererOptions;

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		DrawDebugPanel();
	}

	bool ImGuiDebugBackend::HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!m_IsInitialized || !m_ImGuiContext)
			return false;

		ImGui::SetCurrentContext(m_ImGuiContext);

		if (ProcessEngineInput(msg, wParam, lParam))
			return true;

		const bool handlerConsumed = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam) != 0;

		ImGuiIO& io = ImGui::GetIO();
		m_WantsMouseCapture = io.WantCaptureMouse;
		m_WantsKeyboardCapture = io.WantCaptureKeyboard;

		const bool mouseConsumed = IsMouseMessage(msg) && m_WantsMouseCapture;
		const bool keyboardConsumed = IsKeyboardMessage(msg) && m_WantsKeyboardCapture;
		return handlerConsumed || mouseConsumed || keyboardConsumed;
	}

	void ImGuiDebugBackend::Render(const UIRenderContext& context)
	{
		if (!m_FrameStarted) return;

		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();
		if (!drawData)
			return;

		const float logicalWidth = context.LogicalWidth > 0 ? static_cast<float>(context.LogicalWidth) : drawData->DisplaySize.x;
		const float logicalHeight = context.LogicalHeight > 0 ? static_cast<float>(context.LogicalHeight) : drawData->DisplaySize.y;
		if (logicalWidth > 0.0f && logicalHeight > 0.0f)
			drawData->DisplaySize = ImVec2(logicalWidth, logicalHeight);

		float framebufferScaleX = 1.0f;
		float framebufferScaleY = 1.0f;
		if (logicalWidth > 0.0f)
			framebufferScaleX = context.Viewport.Width / logicalWidth;
		if (logicalHeight > 0.0f)
			framebufferScaleY = context.Viewport.Height / logicalHeight;
		drawData->FramebufferScale = ImVec2(framebufferScaleX, framebufferScaleY);

		context.CommandList->SetDescriptorHeaps(1, m_ImGuiSrvHeap.GetAddressOf());
		ImGui_ImplDX12_RenderDrawData(drawData, context.CommandList);
	}

	void ImGuiDebugBackend::EndFrame()
	{
		if (!m_FrameStarted) return;

		m_FrameStarted = false;
		m_FrameContext->DebugSnapshot.ApplyRendererOptions(&m_LocalRendererOptions);
	}

	void ImGuiDebugBackend::OnResize(uint32_t width, uint32_t height)
	{
	}

	bool ImGuiDebugBackend::WantsKeyboardCapture() const
	{
		if (!m_IsInitialized || !m_Visible)
			return false;
		return m_IsInitialized && m_WantsKeyboardCapture;
	}

	bool ImGuiDebugBackend::WantsMouseCapture() const
	{
		if (!m_IsInitialized || !m_Visible)
			return false;
		return m_IsInitialized && m_WantsMouseCapture;
	}

	bool ImGuiDebugBackend::IsEnabled() const
	{
		return m_IsInitialized;
	}

	void ImGuiDebugBackend::DrawDebugPanel()
	{
		ImGui::Begin("Debug Panel");

		UIDebugSnapshot snapshot = m_FrameContext->DebugSnapshot;
		ImGui::Text(std::format("FPS: {0:.2f}", snapshot.FPS).c_str());
		ImGui::Text(std::format("Frame time: {0:.2f}ms", snapshot.FrameTimeMs).c_str());
		ImGui::Text(std::format("Drawn primitives: {0}", snapshot.DrawnPrimitiveCount).c_str());
		ImGui::Text(std::format("Active animations: {0}", snapshot.ActiveAnimationCount).c_str());
		auto cameraPos = snapshot.CameraPosition;
		ImGui::Text(std::format("Camera Pos: x: {0:.2f}, y: {1:.2f}, z: {2:.2f}", cameraPos.x, cameraPos.y, cameraPos.z).c_str());

		if (ImGui::CollapsingHeader("Renderer Options"))
		{
			DrawRendererOptions(m_LocalRendererOptions);
		}

		ImGui::End();
	}

	bool ImGuiDebugBackend::ProcessEngineInput(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		InputController::IsNewKeyPress(msg, wParam, lParam, [this](WPARAM key) {
			switch (key)
			{
			case VK_F1:
				m_Visible = !m_Visible;
				std::cout << "Toggled debug panel visibility: " << (m_Visible ? "Visible" : "Hidden") << std::endl;
				return true;
			}
		});
		return false;
	}
}
