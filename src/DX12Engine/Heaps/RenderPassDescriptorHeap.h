#pragma once
#include "d3dx12.h"
#include "DescriptorHeap.h"
#include "DescriptorHeapHandle.h"

namespace DX12Engine
{
	class RenderPassDescriptorHeap : public DescriptorHeap
	{
	public:
		RenderPassDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors);
		~RenderPassDescriptorHeap() final;

		void Reset();
		DescriptorHeapHandle GetHeapHandleBlock(UINT count);

	private:
		UINT m_CurrentDescriptorIndex;
	};
}

