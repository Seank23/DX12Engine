#include "VertexBuffer.h"

namespace DX12Engine
{

    VertexBuffer::VertexBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT vertexStride, UINT bufferSize)
		: GPUResource(resource, usageState)
    {
		m_GPUAddress = resource->GetGPUVirtualAddress();
        m_VertexBufferView.BufferLocation = m_GPUAddress;
        m_VertexBufferView.StrideInBytes = vertexStride;
        m_VertexBufferView.SizeInBytes = bufferSize;
    }

    VertexBuffer::~VertexBuffer()
	{
		m_VertexBufferView.BufferLocation = 0;
		m_VertexBufferView.SizeInBytes = 0;
		m_VertexBufferView.StrideInBytes = 0;
	}
}
