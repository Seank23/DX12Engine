#include "Shader.h"
#include <iostream>

namespace DX12Engine
{
	Shader::Shader(std::string shaderPath, std::string shaderType)
	{
		std::wstring widestr = std::wstring(shaderPath.begin(), shaderPath.end());

		IDxcCompiler* dxcCompiler = nullptr;
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		IDxcLibrary* dxcLibrary = nullptr;
		DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxcLibrary));
		IDxcBlobEncoding* sourceBlob = nullptr;
		dxcLibrary->CreateBlobFromFile(widestr.c_str(), nullptr, &sourceBlob);
		IDxcOperationResult* compileResult = nullptr;

		if (shaderType == "vertex")
			dxcCompiler->Compile(sourceBlob, widestr.c_str(), L"main", L"vs_6_0", nullptr, 0, nullptr, 0, nullptr, &compileResult);
		else if (shaderType == "pixel")
			dxcCompiler->Compile(sourceBlob, widestr.c_str(), L"main", L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &compileResult);

		compileResult->GetResult(&m_Shader);
	}

	Shader::~Shader()
	{
		m_Shader.Reset();
	}
}
