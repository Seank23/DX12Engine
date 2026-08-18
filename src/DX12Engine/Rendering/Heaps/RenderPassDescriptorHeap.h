#pragma once
#include "d3dx12.h"
#include "DescriptorHeap.h"
#include "DescriptorHeapHandle.h"
#include "../../Utils/Constants.h"

namespace DX12Engine
{
	class RenderPassDescriptorHeap : public DescriptorHeap
	{
	public:
		RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT transientCapacityPerFrame);
		~RenderPassDescriptorHeap() final;

		// Resets the transient cursor for frameIndex and returns a block of count handles.
		void BeginFrame(UINT frameIndex);
		DescriptorHeapHandle AllocateHandleBlock(UINT count);

		virtual DescriptorHeapStats GetStats() const;

	private:
		UINT m_TransientCapacityPerFrame;
		UINT m_TransientCursors[FRAMES_IN_FLIGHT];
		UINT m_TransientPeak[FRAMES_IN_FLIGHT];
		UINT m_CurrentFrameIndex;
		UINT m_AllocationFailures;
	};
}
