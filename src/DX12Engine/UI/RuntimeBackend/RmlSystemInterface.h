#pragma once

#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/Log.h>
#include <RmlUi/Core/SystemInterface.h>

#include <filesystem>
#include <wtypes.h>

namespace DX12Engine
{
	class RmlSystemInterfaceWin32 final : public Rml::SystemInterface
	{
	public:
		explicit RmlSystemInterfaceWin32(HWND windowHandle = nullptr);

		void SetWindowHandle(HWND windowHandle);

		double GetElapsedTime() override;
		bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;
		void SetMouseCursor(const Rml::String& cursor_name) override;

	private:
		HWND m_WindowHandle = nullptr;
		double m_TimeFrequency = 0.0;
		LARGE_INTEGER m_TimeStartup = {};
	};

	class RmlFileInterface final : public Rml::FileInterface
	{
	public:
		explicit RmlFileInterface(std::filesystem::path rootPath);

		Rml::FileHandle Open(const Rml::String& path) override;
		void Close(Rml::FileHandle file) override;
		size_t Read(void* buffer, size_t size, Rml::FileHandle file) override;
		bool Seek(Rml::FileHandle file, long offset, int origin) override;
		size_t Tell(Rml::FileHandle file) override;

		const std::filesystem::path& GetRootPath() const { return m_RootPath; }
		std::filesystem::path ResolvePath(const Rml::String& path) const;

	private:
		std::filesystem::path m_RootPath;
	};

	namespace RmlInputWin32
	{
		Rml::Input::KeyIdentifier ConvertKey(int win32KeyCode);
		int GetKeyModifierState();
	}
}
