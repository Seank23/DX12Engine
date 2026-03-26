#define NOMINMAX
#include "RenderPassDescriptorHeap.h"
#include <stdexcept>
#include <algorithm>

namespace DX12Engine
{
	RenderPassDescriptorHeap::RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT transientCapacityPerFrame)
		: DescriptorHeap(device, heapType, transientCapacityPerFrame * FRAMES_IN_FLIGHT, true)
		, m_TransientCapacityPerFrame(transientCapacityPerFrame)
		, m_CurrentFrameIndex(0)
		, m_AllocationFailures(0)
	{
		for (UINT i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			m_TransientCursors[i] = 0;
			m_TransientPeak[i] = 0;
		}
	}

	RenderPassDescriptorHeap::~RenderPassDescriptorHeap()
	{
	}

	void RenderPassDescriptorHeap::BeginFrame(UINT frameIndex)
	{
		m_CurrentFrameIndex = frameIndex % FRAMES_IN_FLIGHT;
		m_TransientCursors[m_CurrentFrameIndex] = 0;
	}

	DescriptorHeapHandle RenderPassDescriptorHeap::AllocateHandleBlock(UINT count)
	{
		UINT& cursor = m_TransientCursors[m_CurrentFrameIndex];
		if (cursor + count > m_TransientCapacityPerFrame)
		{
			m_AllocationFailures++;
			throw std::runtime_error("Ran out of transient descriptor heap handles for this frame, need to increase transient capacity.");
		}

		// Transient region starts right after the persistent region, offset per frame slot.
		UINT frameOffset = m_CurrentFrameIndex * m_TransientCapacityPerFrame;
		UINT index = frameOffset + cursor;
		cursor += count;

		m_TransientPeak[m_CurrentFrameIndex] = std::max(m_TransientPeak[m_CurrentFrameIndex], cursor);

		DescriptorHeapHandle handle;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_DescriptorHeapCPUStart;
		cpuHandle.ptr += index * m_DescriptorSize;
		handle.SetCPUHandle(cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_DescriptorHeapGPUStart;
		gpuHandle.ptr += index * m_DescriptorSize;
		handle.SetGPUHandle(gpuHandle);

		handle.SetHeapIndex(index);
		return handle;
	}

	DescriptorHeapStats RenderPassDescriptorHeap::GetStats() const
	{
		UINT peakTransient = 0;
		for (UINT i = 0; i < FRAMES_IN_FLIGHT; i++)
			peakTransient = std::max(peakTransient, m_TransientPeak[i]);

		DescriptorHeapStats stats{};
		stats.transientUsedThisFrame = m_TransientCursors[m_CurrentFrameIndex];
		stats.transientPeakThisFrame = peakTransient;
		stats.transientCapacityPerFrame = m_TransientCapacityPerFrame;
		stats.allocationFailures = m_AllocationFailures;
		return stats;
	}
}

