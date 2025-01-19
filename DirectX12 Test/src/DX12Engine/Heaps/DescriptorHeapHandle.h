#pragma once
#include "d3dx12.h"

namespace DX12Engine
{
    class DescriptorHeapHandle
    {
    public:
        DescriptorHeapHandle()
        {
            m_CPUHandle.ptr = NULL;
            m_GPUHandle.ptr = NULL;
            m_HeapIndex = 0;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return m_CPUHandle; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_GPUHandle; }
        UINT GetHeapIndex() const { return m_HeapIndex; }

        void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) { m_CPUHandle = cpuHandle; }
        void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) { m_GPUHandle = gpuHandle; }
        void SetHeapIndex(UINT heapIndex) { m_HeapIndex = heapIndex; }

        bool IsValid() const { return m_CPUHandle.ptr != NULL; }
        bool IsReferencedByShader() const { return m_GPUHandle.ptr != NULL; }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle;
        UINT m_HeapIndex;
    };
}
