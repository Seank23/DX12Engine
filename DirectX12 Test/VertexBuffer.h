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
	struct Vertex 
	{
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	class VertexBuffer
	{
	public:
		VertexBuffer();
		~VertexBuffer();

		void SetVertices(Microsoft::WRL::ComPtr<ID3D12Device> device, std::vector<Vertex>& vertices);

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_VertexBufferView; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Resource> m_VertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	};
}

