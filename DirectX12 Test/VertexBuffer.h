#pragma once
#include "d3dx12.h"
#include "GPUResource.h"

namespace DX12Engine
{
	class VertexBuffer : GPUResource
	{
	public:
		VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT vertexStride, UINT bufferSize);
		~VertexBuffer();

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_VertexBufferView; }

	private:
		D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	};
}

