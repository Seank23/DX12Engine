#pragma once
#include "d3dx12.h"
#include "../Resources/GPUResource.h"

namespace DX12Engine
{
	class IndexBuffer : public GPUResource
	{
	public:
		IndexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DXGI_FORMAT format, UINT bufferSize);
		~IndexBuffer();

		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_IndexBufferView; }

	private:
		D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
	};
}

