#include "RmlUIBackend.h"
#include "../UIContext.h"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <cctype>
#include <unordered_set>
#include <string_view>
#include <windowsx.h>

namespace DX12Engine
{
	namespace
	{
		constexpr uint8_t MouseButtonBit(const int buttonIndex)
		{
			return (buttonIndex >= 0 && buttonIndex < 8) ? static_cast<uint8_t>(1u << buttonIndex) : 0u;
		}

		bool ShouldCaptureKeyboard(Rml::Context* context)
		{
			if (!context)
				return false;

			Rml::Element* const focusElement = context->GetFocusElement();
			if (!focusElement)
				return false;

			if (focusElement == focusElement->GetOwnerDocument())
				return false;

			const Rml::String tagName = focusElement->GetTagName();
			if (tagName == "body" || tagName == "html")
				return false;

			return true;
		}
	}

	RmlUIBackend::RmlUIBackend()
	{
	}

	RmlUIBackend::~RmlUIBackend()
	{
		Shutdown();
	}

	bool RmlUIBackend::Initialize(const UIConfig& config)
	{
		if (m_IsInitialized)
			return true;

		if (!config.EngineRenderContext || !config.WindowHandle)
			return false;

		m_SystemInterface = std::make_unique<RmlSystemInterfaceWin32>(config.WindowHandle);
		m_FileInterface = std::make_unique<RmlFileInterface>(m_UIRootPath);
		m_RenderInterface = std::make_unique<RmlRenderInterfaceDX12>();
		if (!m_RenderInterface->Initialize(config.EngineRenderContext, m_UIRootPath))
		{
			m_RenderInterface.reset();
			m_FileInterface.reset();
			m_SystemInterface.reset();
			return false;
		}

		Rml::SetSystemInterface(m_SystemInterface.get());
		Rml::SetFileInterface(m_FileInterface.get());
		Rml::SetRenderInterface(m_RenderInterface.get());

		if (!Rml::Initialise())
		{
			m_RenderInterface->Shutdown();
			m_RenderInterface.reset();
			m_FileInterface.reset();
			m_SystemInterface.reset();
			return false;
		}
		m_RmlInitialized = true;

		Rml::Vector2i dimensions(
			static_cast<int>((std::max)(config.InitialWidth, 1u)),
			static_cast<int>((std::max)(config.InitialHeight, 1u)));
		m_Context = Rml::CreateContext(m_ContextName, dimensions);
		if (!m_Context)
		{
			Shutdown();
			return false;
		}
		m_Context->EnableMouseCursor(true);

		const std::array<std::string, 3> startupFonts = {
			"Fonts/LatoLatin-Regular.ttf",
			"Fonts/Roboto-Regular.ttf",
			"Fonts/NotoSans-Regular.ttf",
		};
		bool loadedFont = false;
		const std::array<std::string_view, 3> startupFamilies = {
			"LatoLatin",
			"Roboto",
			"NotoSans",
		};
		for (const std::string& fontPath : startupFonts)
		{
			const std::filesystem::path fullPath = m_UIRootPath / fontPath;
			if (std::filesystem::exists(fullPath))
			{
				const bool loaded = Rml::LoadFontFace(fontPath);
				loadedFont = loaded || loadedFont;
			}
		}

		if (!loadedFont)
		{
			const std::array<std::filesystem::path, 3> fallbackFonts = {
				"C:/Windows/Fonts/segoeui.ttf",
				"C:/Windows/Fonts/arial.ttf",
				"C:/Windows/Fonts/tahoma.ttf",
			};

			for (size_t index = 0; index < fallbackFonts.size(); ++index)
			{
				const auto& absolutePath = fallbackFonts[index];
				if (std::filesystem::exists(absolutePath))
				{
					const std::string absolutePathString = absolutePath.generic_string();
					const bool loadedWithAlias =
						Rml::LoadFontFace(absolutePathString, std::string(startupFamilies[index]), Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Normal, true);
					loadedFont = loadedWithAlias || loadedFont;

					if (loadedFont)
						continue;

					const bool loadedFallback = Rml::LoadFontFace(absolutePathString, true);
					loadedFont = loadedFallback || loadedFont;
				}
			}
		}

		const std::array<std::string, 4> startupDocuments = {
			"hud.rml",
			"default.rml",
			"index.rml",
			"ui.rml"
		};
		for (const std::string& documentPath : startupDocuments)
		{
			if (ShowDocument(documentPath))
				break;
		}

		m_IsInitialized = true;
		return true;
	}

	void RmlUIBackend::Shutdown()
	{
		if (!m_IsInitialized && !m_RmlInitialized)
			return;

		if (m_Context)
		{
			Rml::RemoveContext(m_ContextName);
			m_Context = nullptr;
		}

		if (m_RmlInitialized)
		{
			Rml::Shutdown();
			m_RmlInitialized = false;
		}

		if (m_RenderInterface)
		{
			m_RenderInterface->Shutdown();
			m_RenderInterface.reset();
		}

		m_FileInterface.reset();
		m_SystemInterface.reset();

		Rml::SetRenderInterface(nullptr);
		Rml::SetFileInterface(nullptr);
		Rml::SetSystemInterface(nullptr);

		m_WantsKeyboardCapture = false;
		m_WantsMouseCapture = false;
		m_MouseCapturedByUi = false;
		m_UiCapturedMouseButtonMask = 0;
		m_DocumentMap.clear();
		m_IsInitialized = false;
	}

	void RmlUIBackend::BeginFrame(const UIFrameContext& context)
	{
		if (!m_IsInitialized || !m_Context)
			return;

		if (context.ScreenWidth > 0 && context.ScreenHeight > 0)
			m_Context->SetDimensions(Rml::Vector2i(static_cast<int>(context.ScreenWidth), static_cast<int>(context.ScreenHeight)));

		m_Context->Update();
		m_WantsKeyboardCapture = ShouldCaptureKeyboard(m_Context);
		m_WantsMouseCapture = m_MouseCapturedByUi;
	}

	bool RmlUIBackend::HandleWindowEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!m_IsInitialized || !m_Context)
			return false;

		if (m_SystemInterface)
			m_SystemInterface->SetWindowHandle(hwnd);

		const int modifiers = RmlInputWin32::GetKeyModifierState();
		bool consumed = false;

		auto processMouseDown = [&](const int buttonIndex, const bool affectsGameMouseCapture)
		{
			consumed = !m_Context->ProcessMouseButtonDown(buttonIndex, modifiers);
			if (!consumed || !affectsGameMouseCapture)
				return;

			const uint8_t bit = MouseButtonBit(buttonIndex);
			if (bit == 0)
				return;

			const bool wasCaptured = m_MouseCapturedByUi;
			m_UiCapturedMouseButtonMask = static_cast<uint8_t>(m_UiCapturedMouseButtonMask | bit);
			m_MouseCapturedByUi = (m_UiCapturedMouseButtonMask != 0);
			if (!wasCaptured)
				SetCapture(hwnd);
		};

		auto processMouseUp = [&](const int buttonIndex, const bool affectsGameMouseCapture)
		{
			consumed = !m_Context->ProcessMouseButtonUp(buttonIndex, modifiers);
			if (!affectsGameMouseCapture)
				return;

			const uint8_t bit = MouseButtonBit(buttonIndex);
			if (bit != 0)
				m_UiCapturedMouseButtonMask = static_cast<uint8_t>(m_UiCapturedMouseButtonMask & ~bit);

			const bool stillCaptured = (m_UiCapturedMouseButtonMask != 0);
			if (m_MouseCapturedByUi && !stillCaptured)
				ReleaseCapture();
			m_MouseCapturedByUi = stillCaptured;
		};

		switch (msg)
		{
		case WM_SIZE:
			OnResize(static_cast<uint32_t>(LOWORD(lParam)), static_cast<uint32_t>(HIWORD(lParam)));
			break;
		case WM_MOUSEMOVE:
			consumed = !m_Context->ProcessMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), modifiers);
			break;
		case WM_MOUSELEAVE:
			consumed = !m_Context->ProcessMouseLeave();
			break;
		case WM_LBUTTONDOWN:
			processMouseDown(0, true);
			break;
		case WM_RBUTTONDOWN:
			processMouseDown(1, false);
			break;
		case WM_MBUTTONDOWN:
			processMouseDown(2, true);
			break;
		case WM_LBUTTONUP:
			processMouseUp(0, true);
			break;
		case WM_RBUTTONUP:
			processMouseUp(1, false);
			break;
		case WM_MBUTTONUP:
			processMouseUp(2, true);
			break;
		case WM_XBUTTONDOWN:
			processMouseDown(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4, true);
			break;
		case WM_XBUTTONUP:
			processMouseUp(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? 3 : 4, true);
			break;
		case WM_CAPTURECHANGED:
			m_MouseCapturedByUi = false;
			m_UiCapturedMouseButtonMask = 0;
			break;
		case WM_MOUSEWHEEL:
			consumed = !m_Context->ProcessMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA), modifiers);
			break;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			consumed = !m_Context->ProcessKeyDown(RmlInputWin32::ConvertKey(static_cast<int>(wParam)), modifiers);
			break;
		case WM_KEYUP:
		case WM_SYSKEYUP:
			consumed = !m_Context->ProcessKeyUp(RmlInputWin32::ConvertKey(static_cast<int>(wParam)), modifiers);
			break;
		case WM_CHAR:
			consumed = !m_Context->ProcessTextInput(static_cast<Rml::Character>(wParam));
			break;
		case WM_UNICHAR:
			if (wParam == UNICODE_NOCHAR)
				return true;
			consumed = !m_Context->ProcessTextInput(static_cast<Rml::Character>(wParam));
			break;
		default:
			break;
		}

		m_WantsKeyboardCapture = ShouldCaptureKeyboard(m_Context);
		m_WantsMouseCapture = m_MouseCapturedByUi;
		return consumed;
	}

	void RmlUIBackend::Render(const UIRenderContext& context)
	{
		if (!m_IsInitialized || !m_Context || !m_RenderInterface)
			return;

		m_RenderInterface->BeginFrame(context);
		m_Context->Render();
		m_RenderInterface->EndFrame();

		m_WantsKeyboardCapture = ShouldCaptureKeyboard(m_Context);
		m_WantsMouseCapture = m_MouseCapturedByUi;
	}

	void RmlUIBackend::EndFrame()
	{
		if (!m_IsInitialized || !m_Context || m_DocumentMap.empty())
			return;

		std::unordered_set<Rml::ElementDocument*> activeDocuments;
		const int numDocuments = m_Context->GetNumDocuments();
		activeDocuments.reserve(static_cast<size_t>((std::max)(numDocuments, 0)));
		for (int index = 0; index < numDocuments; ++index)
		{
			if (Rml::ElementDocument* document = m_Context->GetDocument(index))
				activeDocuments.insert(document);
		}

		for (auto it = m_DocumentMap.begin(); it != m_DocumentMap.end();)
		{
			if (activeDocuments.find(it->second) == activeDocuments.end())
				it = m_DocumentMap.erase(it);
			else
				++it;
		}
	}

	void RmlUIBackend::OnResize(uint32_t width, uint32_t height)
	{
		if (!m_Context || width == 0 || height == 0)
			return;

		m_Context->SetDimensions(Rml::Vector2i(static_cast<int>(width), static_cast<int>(height)));
	}

	bool RmlUIBackend::WantsKeyboardCapture() const
	{
		return m_IsInitialized && m_WantsKeyboardCapture;
	}

	bool RmlUIBackend::WantsMouseCapture() const
	{
		return m_IsInitialized && m_WantsMouseCapture;
	}

	bool RmlUIBackend::IsEnabled() const
	{
		return m_IsInitialized;
	}

	bool RmlUIBackend::ShowDocument(const std::string& documentPath)
	{
		if (!m_Context || documentPath.empty())
			return false;

		Rml::ElementDocument* document = GetDocument(documentPath);
		if (!document)
		{
			const std::string requestedKey = NormalizeDocumentKey(documentPath);
			const std::string requestedFilename = NormalizeDocumentKey(std::filesystem::path(documentPath).filename().string());
			const int numDocuments = m_Context->GetNumDocuments();
			for (int index = 0; index < numDocuments; ++index)
			{
				Rml::ElementDocument* candidate = m_Context->GetDocument(index);
				if (!candidate)
					continue;

				const std::string sourceKey = NormalizeDocumentKey(candidate->GetSourceURL());
				if (sourceKey == requestedKey)
				{
					document = candidate;
					break;
				}

				if (!requestedFilename.empty())
				{
					const std::string sourceFilename = NormalizeDocumentKey(std::filesystem::path(candidate->GetSourceURL()).filename().string());
					if (sourceFilename == requestedFilename)
					{
						document = candidate;
						break;
					}
				}
			}
		}

		if (!document)
		{
			document = m_Context->LoadDocument(documentPath);
			if (!document)
				return false;
			RegisterDocumentPath(documentPath, document);
		}
		else
		{
			RegisterDocumentPath(documentPath, document);
		}

		document->Show();
		m_Context->PullDocumentToFront(document);
		return true;
	}

	bool RmlUIBackend::HideDocument(const std::string& documentPathOrId)
	{
		if (!m_Context)
			return false;

		Rml::ElementDocument* document = GetDocument(documentPathOrId);
		if (!document)
			return false;

		document->Hide();
		return true;
	}

	Rml::ElementDocument* RmlUIBackend::GetDocument(const std::string& documentPathOrId) const
	{
		if (!m_Context || documentPathOrId.empty())
			return nullptr;

		const std::string normalized = NormalizeDocumentKey(documentPathOrId);
		auto mapIt = m_DocumentMap.find(normalized);
		if (mapIt != m_DocumentMap.end())
			return mapIt->second;

		return m_Context->GetDocument(documentPathOrId);
	}

	bool RmlUIBackend::DispatchEvent(const std::string& documentPathOrId, const std::string& eventName)
	{
		if (!m_Context || eventName.empty())
			return false;

		Rml::ElementDocument* document = GetDocument(documentPathOrId);
		if (!document)
			return false;

		Rml::Dictionary emptyParameters;
		return document->DispatchEvent(eventName, emptyParameters);
	}

	std::string RmlUIBackend::NormalizeDocumentKey(const std::string& key)
	{
		std::string result = key;
		std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch)
		{
			if (ch == '\\')
				return '/';
			return static_cast<char>(std::tolower(ch));
		});
		return result;
	}

	void RmlUIBackend::RegisterDocumentPath(const std::string& documentPath, Rml::ElementDocument* document)
	{
		if (!documentPath.empty() && document)
			m_DocumentMap[NormalizeDocumentKey(documentPath)] = document;
	}
}
