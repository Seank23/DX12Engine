#pragma once
#include <wrl.h>
#include <dxcapi.h>
#include <string>
#include <filesystem>

namespace DX12Engine
{
	enum class ShaderType
	{
		Vertex,
		Pixel,
		Compute
	};

	class Shader
	{
	public:
		Shader(std::string shaderPath, ShaderType shaderType);
		~Shader();

		const Microsoft::WRL::ComPtr<IDxcBlob> GetShader() { return m_Shader; }
		const std::string& GetLastError() const { return m_LastError; }

		bool Compile();
		bool ReloadIfChanged();

	private:
		Microsoft::WRL::ComPtr<IDxcBlob> m_Shader;

		std::wstring m_ShaderPath;
		ShaderType m_Type;
		std::string m_LastError;
		std::filesystem::file_time_type m_LastModifiedTime;
	};
}
