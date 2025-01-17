#include "StagingDescriptorHeap.h"
#include <stdexcept>

namespace DX12Engine
{
    StagingDescriptorHeap::StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
        : DescriptorHeap(device, heapType, numDescriptors, false)
    {
        m_CurrentDescriptorIndex = 0;
        m_ActiveHandleCount = 0;
    }

    StagingDescriptorHeap::~StagingDescriptorHeap()
    {
        if (m_ActiveHandleCount != 0)
            std::runtime_error("There were active handles when the descriptor heap was destroyed. Look for leaks.");

        m_FreeDescriptors.clear();
    }

    DescriptorHeapHandle StagingDescriptorHeap::GetNewHeapHandle()
    {
        UINT newHandleID = 0;

        if (m_CurrentDescriptorIndex < m_MaxDescriptors)
        {
            newHandleID = m_CurrentDescriptorIndex;
            m_CurrentDescriptorIndex++;
        }
        else if (m_FreeDescriptors.size() > 0)
        {
			newHandleID = m_FreeDescriptors.back();
            m_FreeDescriptors.pop_back();
        }
        else
        {
            std::runtime_error("Ran out of dynamic descriptor heap handles, need to increase heap size.");
        }

        DescriptorHeapHandle newHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_DescriptorHeapCPUStart;
        cpuHandle.ptr += newHandleID * m_DescriptorSize;
        newHandle.SetCPUHandle(cpuHandle);
        newHandle.SetHeapIndex(newHandleID);
        m_ActiveHandleCount++;

        return newHandle;
    }

    void StagingDescriptorHeap::FreeHeapHandle(DescriptorHeapHandle handle)
    {
        m_FreeDescriptors.push_back(handle.GetHeapIndex());

        if (m_ActiveHandleCount == 0)
        {
            std::runtime_error("Freeing heap handles when there should be none left");
        }
        m_ActiveHandleCount--;
    }
}
