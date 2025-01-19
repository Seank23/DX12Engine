#include "Shader.h"

namespace DX12Engine
{
	Shader::Shader(std::string shaderPath, std::string shaderType)
	{
		std::wstring widestr = std::wstring(shaderPath.begin(), shaderPath.end());
		if (shaderType == "vertex")
			D3DCompileFromFile(widestr.c_str(), nullptr, nullptr, "main", "vs_5_0", 0, 0, &m_Shader, nullptr);
		else if (shaderType == "pixel")
			D3DCompileFromFile(widestr.c_str(), nullptr, nullptr, "main", "ps_5_0", 0, 0, &m_Shader, nullptr);
	}

	Shader::~Shader()
	{
		m_Shader.Reset();
	}
}
