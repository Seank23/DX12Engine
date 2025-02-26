#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include "../Resources/Shader.h"
#include "../Buffers/ConstantBuffer.h"

namespace DX12Engine
{
	class RenderDevice
	{
	public:
		RenderDevice();
		~RenderDevice();

		void Init(HWND hwnd);

		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return m_Device; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
	};
}
