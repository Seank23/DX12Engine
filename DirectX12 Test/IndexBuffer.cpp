#include "IndexBuffer.h"

namespace DX12Engine
{
    IndexBuffer::IndexBuffer()
    {
    }

    IndexBuffer::~IndexBuffer()
    {
    }

    void IndexBuffer::SetData(Microsoft::WRL::ComPtr<ID3D12Device> device, std::vector<UINT16>& indices)
	{
        const UINT indexBufferSize = sizeof(UINT16) * indices.size();

        // Create the index buffer resource in the GPU's default heap
        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
        device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_IndexBuffer));

        // Copy the indices to the index buffer
        void* indexDataBegin = nullptr;
        CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU
        m_IndexBuffer->Map(0, &readRange, &indexDataBegin);
        memcpy(indexDataBegin, &indices[0], indexBufferSize);
        m_IndexBuffer->Unmap(0, nullptr);

        // Initialize the index buffer view
        m_IndexBufferView.BufferLocation = m_IndexBuffer->GetGPUVirtualAddress();
        m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
        m_IndexBufferView.SizeInBytes = indexBufferSize;
	}
}
