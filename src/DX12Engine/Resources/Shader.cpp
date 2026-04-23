#include "Shader.h"
#include <wrl/client.h>
#include <windows.h>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace DX12Engine
{
	Shader::Shader(std::string shaderPath, ShaderType shaderType)
	{
		std::wstring widestr = std::wstring(shaderPath.begin(), shaderPath.end());
		m_ShaderPath = widestr;
		m_Type = shaderType;
		if (!Compile())
			throw std::runtime_error("Failed to compile shader: " + shaderPath + "\n" + m_LastError);

		std::error_code ec;
		auto modifiedTime = std::filesystem::last_write_time(m_ShaderPath, ec);
		m_LastModifiedTime = ec ? (std::filesystem::file_time_type::min)() : modifiedTime;
	}

	Shader::~Shader()
	{
		m_Shader.Reset();
	}

	bool Shader::Compile()
	{
		m_LastError.clear();

		Microsoft::WRL::ComPtr<IDxcCompiler> dxcCompiler;
		HRESULT hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
		if (FAILED(hr) || !dxcCompiler)
		{
			m_LastError = "DxcCreateInstance(CLSID_DxcCompiler) failed.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		Microsoft::WRL::ComPtr<IDxcLibrary> dxcLibrary;
		hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&dxcLibrary));
		if (FAILED(hr) || !dxcLibrary)
		{
			m_LastError = "DxcCreateInstance(CLSID_DxcLibrary) failed.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
		hr = dxcLibrary->CreateBlobFromFile(m_ShaderPath.c_str(), nullptr, &sourceBlob);
		if (FAILED(hr) || !sourceBlob)
		{
			m_LastError = "Failed to load shader source file.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		LPCWSTR targetProfile = nullptr;
		switch (m_Type)
		{
		case ShaderType::Vertex:
			targetProfile = L"vs_6_0";
			break;
		case ShaderType::Pixel:
			targetProfile = L"ps_6_0";
			break;
		case ShaderType::Compute:
			targetProfile = L"cs_6_0";
			break;
		default:
			m_LastError = "Unsupported shader type.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		Microsoft::WRL::ComPtr<IDxcOperationResult> compileResult;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler;
		hr = dxcLibrary->CreateIncludeHandler(&includeHandler);
		if (FAILED(hr) || !includeHandler)
		{
			m_LastError = "Failed to create DXC include handler.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		std::filesystem::path shaderPathFs(m_ShaderPath);
		std::wstring shaderDir = shaderPathFs.parent_path().wstring();
		std::vector<LPCWSTR> compileArgs;
		compileArgs.push_back(L"-I");
		compileArgs.push_back(shaderDir.c_str());

		hr = dxcCompiler->Compile(
			sourceBlob.Get(),
			m_ShaderPath.c_str(),
			L"main",
			targetProfile,
			compileArgs.data(),
			static_cast<UINT32>(compileArgs.size()),
			nullptr,
			0,
			includeHandler.Get(),
			&compileResult);
		if (FAILED(hr) || !compileResult)
		{
			m_LastError = "DXC compile invocation failed.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		HRESULT compileStatus = S_OK;
		compileResult->GetStatus(&compileStatus);

		std::string errorText;
		Microsoft::WRL::ComPtr<IDxcBlobEncoding> errors;
		if (SUCCEEDED(compileResult->GetErrorBuffer(&errors)) && errors && errors->GetBufferSize() > 0)
		{
			errorText.assign(
				reinterpret_cast<const char*>(errors->GetBufferPointer()),
				errors->GetBufferSize());
		}

		if (FAILED(compileStatus))
		{
			if (errorText.empty())
				errorText = "Shader compilation failed with no diagnostic text from DXC.";

			std::ostringstream oss;
			oss << "Shader compile failed for " << std::string(m_ShaderPath.begin(), m_ShaderPath.end()) << "\n" << errorText;
			m_LastError = oss.str();
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		Microsoft::WRL::ComPtr<IDxcBlob> compiledBlob;
		hr = compileResult->GetResult(&compiledBlob);
		if (FAILED(hr) || !compiledBlob)
		{
			m_LastError = "Shader compilation succeeded but no bytecode blob was returned.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		m_Shader = compiledBlob;
		return true;
	}

	bool Shader::ReloadIfChanged()
	{
		std::error_code ec;
		auto lastWriteTime = std::filesystem::last_write_time(m_ShaderPath, ec);
		if (ec)
		{
			m_LastError = "Failed to query shader file timestamp.";
			OutputDebugStringA(("[Shader] " + m_LastError + "\n").c_str());
			return false;
		}

		if (lastWriteTime != m_LastModifiedTime)
		{
			if (Compile())
			{
				m_LastModifiedTime = lastWriteTime;
				return true;
			}
			return false;
		}
		return false;
	}
}
