#pragma once
#include "d3dx12.h"

namespace DX12Engine
{
	class DescriptorHeap
	{
	public:
        DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader);
        virtual ~DescriptorHeap();

        ID3D12DescriptorHeap* GetHeap() { return m_DescriptorHeap; }
        D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return m_HeapType; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetHeapCPUStart() const { return m_DescriptorHeapCPUStart; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetHeapGPUStart() const { return m_DescriptorHeapGPUStart; }
        UINT GetMaxDescriptors() const { return m_MaxDescriptors; }
        UINT GetDescriptorSize() const { return m_DescriptorSize; }

    protected:
        ID3D12DescriptorHeap* m_DescriptorHeap;
        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
        D3D12_CPU_DESCRIPTOR_HANDLE m_DescriptorHeapCPUStart;
        D3D12_GPU_DESCRIPTOR_HANDLE m_DescriptorHeapGPUStart;
        UINT m_MaxDescriptors;
        UINT m_DescriptorSize;
        bool   m_IsReferencedByShader;
	};
}

