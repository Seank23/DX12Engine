#include "IndexBuffer.h"

namespace DX12Engine
{
    IndexBuffer::IndexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DXGI_FORMAT format, UINT bufferSize)
		: GPUResource(resource, usageState)
    {
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_IndexBufferView.BufferLocation = m_GPUAddress;
		m_IndexBufferView.Format = format;
		m_IndexBufferView.SizeInBytes = bufferSize;
    }

    IndexBuffer::~IndexBuffer()
    {
		m_IndexBufferView.BufferLocation = 0;
		m_IndexBufferView.Format = (DXGI_FORMAT)NULL;
		m_IndexBufferView.SizeInBytes = 0;
    }
}
