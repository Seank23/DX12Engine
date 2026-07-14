#pragma once
#include "d3dx12.h"
#include "DescriptorHeap.h"
#include "DescriptorHeapHandle.h"

#include <vector>

namespace DX12Engine
{
	class StagingDescriptorHeap : public DescriptorHeap
	{
	public:
		StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~StagingDescriptorHeap() final;

		DescriptorHeapHandle AllocatePersistentHandle();
		void ReleasePersistentHandle(const DescriptorHeapHandle& handle);

		virtual DescriptorHeapStats GetStats() const;

	private:
		UINT m_CurrentDescriptorIndex;
		UINT m_ActivePersistentCount;
		UINT m_AllocationFailures;
		std::vector<UINT> m_FreeDescriptorIndices;
	};
}
