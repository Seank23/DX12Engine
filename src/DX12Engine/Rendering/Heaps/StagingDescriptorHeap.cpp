#include "StagingDescriptorHeap.h"
#include <stdexcept>

namespace DX12Engine
{
    StagingDescriptorHeap::StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
        : DescriptorHeap(device, heapType, numDescriptors, false)
    {
        m_CurrentDescriptorIndex = 0;
        m_AllocationFailures = 0;
    }

    StagingDescriptorHeap::~StagingDescriptorHeap()
    {
    }

    DescriptorHeapHandle StagingDescriptorHeap::AllocatePersistentHandle()
    {
        UINT newHandleID = 0;

        if (m_CurrentDescriptorIndex < m_MaxDescriptors)
        {
            newHandleID = m_CurrentDescriptorIndex;
            m_CurrentDescriptorIndex++;
        }
        else
        {
            m_AllocationFailures++;
            throw std::runtime_error("Ran out of dynamic descriptor heap handles, need to increase heap size.");
        }

        DescriptorHeapHandle newHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_DescriptorHeapCPUStart;
        cpuHandle.ptr += newHandleID * m_DescriptorSize;
        newHandle.SetCPUHandle(cpuHandle);
        newHandle.SetHeapIndex(newHandleID);

        return newHandle;
    }

    DescriptorHeapStats StagingDescriptorHeap::GetStats() const
    {
        DescriptorHeapStats stats{};
		stats.persistentCapacity = m_MaxDescriptors;
		stats.persistentUsed = m_CurrentDescriptorIndex;
        return stats;
    }
}
