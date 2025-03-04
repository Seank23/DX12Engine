#pragma once
#include <wrl.h>
#include <dxcapi.h>
#include <string>

namespace DX12Engine
{
	class Shader
	{
	public:
		Shader(std::string shaderPath, std::string shaderType);
		~Shader();

		const Microsoft::WRL::ComPtr<IDxcBlob> GetShader() { return m_Shader; }

	private:
		Microsoft::WRL::ComPtr<IDxcBlob> m_Shader;
	};
}

