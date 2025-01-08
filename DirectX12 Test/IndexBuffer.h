#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include "RenderDevice.h"

namespace DX12Engine
{
	class IndexBuffer
	{
	public:
		IndexBuffer();
		~IndexBuffer();

		void SetData(Microsoft::WRL::ComPtr<ID3D12Device> device, std::vector<UINT16>& indices);

		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_IndexBufferView; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_IndexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
	};
}

