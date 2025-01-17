#include "RenderPassDescriptorHeap.h"
#include <stdexcept>

namespace DX12Engine
{
	RenderPassDescriptorHeap::RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors)
		: DescriptorHeap(device, heapType, numDescriptors, true)
	{
		m_CurrentDescriptorIndex = 0;
	}

	RenderPassDescriptorHeap::~RenderPassDescriptorHeap()
	{
	}

	void RenderPassDescriptorHeap::Reset()
	{
		m_CurrentDescriptorIndex = 0;
	}

	DescriptorHeapHandle RenderPassDescriptorHeap::GetHeapHandleBlock(UINT count)
	{
        UINT newHandleID = 0;
        UINT blockEnd = m_CurrentDescriptorIndex + count;

        if (blockEnd < m_MaxDescriptors)
        {
            newHandleID = m_CurrentDescriptorIndex;
            m_CurrentDescriptorIndex = blockEnd;
        }
        else
        {
            std::runtime_error("Ran out of render pass descriptor heap handles, need to increase heap size.");
        }

        DescriptorHeapHandle newHandle;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_DescriptorHeapCPUStart;
        cpuHandle.ptr += newHandleID * m_DescriptorSize;
        newHandle.SetCPUHandle(cpuHandle);

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_DescriptorHeapGPUStart;
        gpuHandle.ptr += newHandleID * m_DescriptorSize;
        newHandle.SetGPUHandle(gpuHandle);

        newHandle.SetHeapIndex(newHandleID);

        return newHandle;
	}
}
