#pragma once
#include "d3dx12.h"
#include "DescriptorHeap.h"
#include "DescriptorHeapHandle.h"

namespace DX12Engine
{
	class StagingDescriptorHeap : public DescriptorHeap
	{
	public:
		StagingDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~StagingDescriptorHeap() final;

		DescriptorHeapHandle GetNewHeapHandle();
		void FreeHeapHandle(DescriptorHeapHandle handle);

	private:
		std::vector<UINT> m_FreeDescriptors;
		UINT m_CurrentDescriptorIndex;
		UINT m_ActiveHandleCount;
	};
}

