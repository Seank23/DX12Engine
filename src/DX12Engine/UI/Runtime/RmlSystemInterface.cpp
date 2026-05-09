#include "RmlSystemInterface.h"

#include <RmlUi/Core/StringUtilities.h>

#include <cstdio>
#include <string>
#include <windows.h>

namespace DX12Engine
{
	RmlSystemInterfaceWin32::RmlSystemInterfaceWin32(HWND windowHandle)
		: m_WindowHandle(windowHandle)
	{
		LARGE_INTEGER timeTicksPerSecond = {};
		QueryPerformanceFrequency(&timeTicksPerSecond);
		QueryPerformanceCounter(&m_TimeStartup);
		m_TimeFrequency = 1.0 / static_cast<double>(timeTicksPerSecond.QuadPart);
	}

	void RmlSystemInterfaceWin32::SetWindowHandle(HWND windowHandle)
	{
		m_WindowHandle = windowHandle;
	}

	double RmlSystemInterfaceWin32::GetElapsedTime()
	{
		LARGE_INTEGER now = {};
		QueryPerformanceCounter(&now);
		return static_cast<double>(now.QuadPart - m_TimeStartup.QuadPart) * m_TimeFrequency;
	}

	bool RmlSystemInterfaceWin32::LogMessage(Rml::Log::Type type, const Rml::String& message)
	{
		const char* prefix = "[Rml]";
		switch (type)
		{
		case Rml::Log::LT_ERROR: prefix = "[Rml][Error]"; break;
		case Rml::Log::LT_WARNING: prefix = "[Rml][Warn]"; break;
		case Rml::Log::LT_INFO: prefix = "[Rml][Info]"; break;
		case Rml::Log::LT_DEBUG: prefix = "[Rml][Debug]"; break;
		case Rml::Log::LT_ASSERT: prefix = "[Rml][Assert]"; break;
		case Rml::Log::LT_ALWAYS: prefix = "[Rml][Always]"; break;
		case Rml::Log::LT_MAX: break;
		}

		std::string line = std::string(prefix) + " " + message + "\n";
		OutputDebugStringA(line.c_str());
		return true;
	}

	void RmlSystemInterfaceWin32::SetMouseCursor(const Rml::String& cursorName)
	{
		if (!m_WindowHandle)
			return;

		HCURSOR cursor = LoadCursor(nullptr, IDC_ARROW);
		if (cursorName == "pointer")
			cursor = LoadCursor(nullptr, IDC_HAND);
		else if (cursorName == "text")
			cursor = LoadCursor(nullptr, IDC_IBEAM);
		else if (cursorName == "move" || Rml::StringUtilities::StartsWith(cursorName, "rmlui-scroll"))
			cursor = LoadCursor(nullptr, IDC_SIZEALL);
		else if (cursorName == "cross")
			cursor = LoadCursor(nullptr, IDC_CROSS);

		SetCursor(cursor);
		SetClassLongPtrA(m_WindowHandle, GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
	}

	RmlFileInterface::RmlFileInterface(std::filesystem::path rootPath)
		: m_RootPath(std::move(rootPath))
	{
	}

	Rml::FileHandle RmlFileInterface::Open(const Rml::String& path)
	{
		const std::filesystem::path resolved = ResolvePath(path);
		if (!resolved.empty())
		{
			if (FILE* file = _wfopen(resolved.wstring().c_str(), L"rb"))
				return reinterpret_cast<Rml::FileHandle>(file);
		}

		if (FILE* file = fopen(path.c_str(), "rb"))
			return reinterpret_cast<Rml::FileHandle>(file);

		return static_cast<Rml::FileHandle>(0);
	}

	void RmlFileInterface::Close(Rml::FileHandle file)
	{
		if (!file)
			return;
		fclose(reinterpret_cast<FILE*>(file));
	}

	size_t RmlFileInterface::Read(void* buffer, size_t size, Rml::FileHandle file)
	{
		if (!file || !buffer || size == 0)
			return 0;
		return fread(buffer, 1, size, reinterpret_cast<FILE*>(file));
	}

	bool RmlFileInterface::Seek(Rml::FileHandle file, long offset, int origin)
	{
		if (!file)
			return false;
		return fseek(reinterpret_cast<FILE*>(file), offset, origin) == 0;
	}

	size_t RmlFileInterface::Tell(Rml::FileHandle file)
	{
		if (!file)
			return 0;
		return static_cast<size_t>(ftell(reinterpret_cast<FILE*>(file)));
	}

	std::filesystem::path RmlFileInterface::ResolvePath(const Rml::String& path) const
	{
		std::filesystem::path sourcePath(path);
		if (sourcePath.is_absolute())
			return sourcePath;

		if (!m_RootPath.empty())
			return m_RootPath / sourcePath;

		return sourcePath;
	}

	namespace RmlInputWin32
	{
		int GetKeyModifierState()
		{
			int modifierState = 0;
			if (GetKeyState(VK_CAPITAL) & 1)
				modifierState |= Rml::Input::KM_CAPSLOCK;
			if (GetKeyState(VK_NUMLOCK) & 1)
				modifierState |= Rml::Input::KM_NUMLOCK;
			if (HIWORD(GetKeyState(VK_SHIFT)) & 1)
				modifierState |= Rml::Input::KM_SHIFT;
			if (HIWORD(GetKeyState(VK_CONTROL)) & 1)
				modifierState |= Rml::Input::KM_CTRL;
			if (HIWORD(GetKeyState(VK_MENU)) & 1)
				modifierState |= Rml::Input::KM_ALT;
			return modifierState;
		}

		Rml::Input::KeyIdentifier ConvertKey(int win32KeyCode)
		{
			switch (win32KeyCode)
			{
			case 'A': return Rml::Input::KI_A;
			case 'B': return Rml::Input::KI_B;
			case 'C': return Rml::Input::KI_C;
			case 'D': return Rml::Input::KI_D;
			case 'E': return Rml::Input::KI_E;
			case 'F': return Rml::Input::KI_F;
			case 'G': return Rml::Input::KI_G;
			case 'H': return Rml::Input::KI_H;
			case 'I': return Rml::Input::KI_I;
			case 'J': return Rml::Input::KI_J;
			case 'K': return Rml::Input::KI_K;
			case 'L': return Rml::Input::KI_L;
			case 'M': return Rml::Input::KI_M;
			case 'N': return Rml::Input::KI_N;
			case 'O': return Rml::Input::KI_O;
			case 'P': return Rml::Input::KI_P;
			case 'Q': return Rml::Input::KI_Q;
			case 'R': return Rml::Input::KI_R;
			case 'S': return Rml::Input::KI_S;
			case 'T': return Rml::Input::KI_T;
			case 'U': return Rml::Input::KI_U;
			case 'V': return Rml::Input::KI_V;
			case 'W': return Rml::Input::KI_W;
			case 'X': return Rml::Input::KI_X;
			case 'Y': return Rml::Input::KI_Y;
			case 'Z': return Rml::Input::KI_Z;
			case '0': return Rml::Input::KI_0;
			case '1': return Rml::Input::KI_1;
			case '2': return Rml::Input::KI_2;
			case '3': return Rml::Input::KI_3;
			case '4': return Rml::Input::KI_4;
			case '5': return Rml::Input::KI_5;
			case '6': return Rml::Input::KI_6;
			case '7': return Rml::Input::KI_7;
			case '8': return Rml::Input::KI_8;
			case '9': return Rml::Input::KI_9;
			case VK_BACK: return Rml::Input::KI_BACK;
			case VK_TAB: return Rml::Input::KI_TAB;
			case VK_RETURN: return Rml::Input::KI_RETURN;
			case VK_ESCAPE: return Rml::Input::KI_ESCAPE;
			case VK_SPACE: return Rml::Input::KI_SPACE;
			case VK_PRIOR: return Rml::Input::KI_PRIOR;
			case VK_NEXT: return Rml::Input::KI_NEXT;
			case VK_END: return Rml::Input::KI_END;
			case VK_HOME: return Rml::Input::KI_HOME;
			case VK_LEFT: return Rml::Input::KI_LEFT;
			case VK_UP: return Rml::Input::KI_UP;
			case VK_RIGHT: return Rml::Input::KI_RIGHT;
			case VK_DOWN: return Rml::Input::KI_DOWN;
			case VK_INSERT: return Rml::Input::KI_INSERT;
			case VK_DELETE: return Rml::Input::KI_DELETE;
			case VK_LWIN: return Rml::Input::KI_LWIN;
			case VK_RWIN: return Rml::Input::KI_RWIN;
			case VK_APPS: return Rml::Input::KI_APPS;
			case VK_NUMPAD0: return Rml::Input::KI_NUMPAD0;
			case VK_NUMPAD1: return Rml::Input::KI_NUMPAD1;
			case VK_NUMPAD2: return Rml::Input::KI_NUMPAD2;
			case VK_NUMPAD3: return Rml::Input::KI_NUMPAD3;
			case VK_NUMPAD4: return Rml::Input::KI_NUMPAD4;
			case VK_NUMPAD5: return Rml::Input::KI_NUMPAD5;
			case VK_NUMPAD6: return Rml::Input::KI_NUMPAD6;
			case VK_NUMPAD7: return Rml::Input::KI_NUMPAD7;
			case VK_NUMPAD8: return Rml::Input::KI_NUMPAD8;
			case VK_NUMPAD9: return Rml::Input::KI_NUMPAD9;
			case VK_MULTIPLY: return Rml::Input::KI_MULTIPLY;
			case VK_ADD: return Rml::Input::KI_ADD;
			case VK_SUBTRACT: return Rml::Input::KI_SUBTRACT;
			case VK_DECIMAL: return Rml::Input::KI_DECIMAL;
			case VK_DIVIDE: return Rml::Input::KI_DIVIDE;
			case VK_F1: return Rml::Input::KI_F1;
			case VK_F2: return Rml::Input::KI_F2;
			case VK_F3: return Rml::Input::KI_F3;
			case VK_F4: return Rml::Input::KI_F4;
			case VK_F5: return Rml::Input::KI_F5;
			case VK_F6: return Rml::Input::KI_F6;
			case VK_F7: return Rml::Input::KI_F7;
			case VK_F8: return Rml::Input::KI_F8;
			case VK_F9: return Rml::Input::KI_F9;
			case VK_F10: return Rml::Input::KI_F10;
			case VK_F11: return Rml::Input::KI_F11;
			case VK_F12: return Rml::Input::KI_F12;
			case VK_SHIFT: return Rml::Input::KI_LSHIFT;
			case VK_CONTROL: return Rml::Input::KI_LCONTROL;
			case VK_MENU: return Rml::Input::KI_LMENU;
			case VK_OEM_PLUS: return Rml::Input::KI_OEM_PLUS;
			case VK_OEM_COMMA: return Rml::Input::KI_OEM_COMMA;
			case VK_OEM_MINUS: return Rml::Input::KI_OEM_MINUS;
			case VK_OEM_PERIOD: return Rml::Input::KI_OEM_PERIOD;
			case VK_OEM_1: return Rml::Input::KI_OEM_1;
			case VK_OEM_2: return Rml::Input::KI_OEM_2;
			case VK_OEM_3: return Rml::Input::KI_OEM_3;
			case VK_OEM_4: return Rml::Input::KI_OEM_4;
			case VK_OEM_5: return Rml::Input::KI_OEM_5;
			case VK_OEM_6: return Rml::Input::KI_OEM_6;
			case VK_OEM_7: return Rml::Input::KI_OEM_7;
			default: return Rml::Input::KI_UNKNOWN;
			}
		}
	}
}
