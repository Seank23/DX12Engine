#include "VertexBuffer.h"

namespace DX12Engine
{

    VertexBuffer::VertexBuffer()
    {

    }

    VertexBuffer::~VertexBuffer()
	{
	}

	void VertexBuffer::SetVertices(Microsoft::WRL::ComPtr<ID3D12Device> device, std::vector<Vertex>& vertices)
	{
        const UINT vertexBufferSize = sizeof(vertices) * vertices.size();

        // Create the vertex buffer resource in the GPU's default heap
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
        device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_VertexBuffer));

        // Copy the triangle vertices to the vertex buffer
        UINT8* vertexDataBegin = nullptr;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
        m_VertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&vertexDataBegin));
        memcpy(vertexDataBegin, &vertices[0], vertexBufferSize);
        m_VertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view
        m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.StrideInBytes = sizeof(Vertex);
        m_VertexBufferView.SizeInBytes = vertexBufferSize;
	}
}
