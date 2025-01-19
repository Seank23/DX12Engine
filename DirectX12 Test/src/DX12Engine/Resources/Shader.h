#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

namespace DX12Engine
{
	class Shader
	{
	public:
		Shader(std::string shaderPath, std::string shaderType);
		~Shader();

		const Microsoft::WRL::ComPtr<ID3DBlob> GetShader() { return m_Shader; }

	private:
		Microsoft::WRL::ComPtr<ID3DBlob> m_Shader;
	};
}

