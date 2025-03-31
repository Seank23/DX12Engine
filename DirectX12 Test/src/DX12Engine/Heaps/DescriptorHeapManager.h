#pragma once
#include "StagingDescriptorHeap.h"
#include "RenderPassDescriptorHeap.h"

namespace DX12Engine
{
	class DescriptorHeapManager
	{
	public:
		DescriptorHeapManager(Microsoft::WRL::ComPtr<ID3D12Device> device);
		~DescriptorHeapManager();

		DescriptorHeapHandle GetNewSRVDescriptorHeapHandle() { return m_StagingHeap->GetNewHeapHandle(); }
		DescriptorHeapHandle GetNewDSVDescriptorHeapHandle() { return m_DepthStencilHeap->GetNewHeapHandle(); }
		DescriptorHeapHandle GetRenderHeapHandleBlock(UINT count) { return m_RenderPassHeap->GetHeapHandleBlock(count); }

		StagingDescriptorHeap& GetStagingHeap() { return *m_StagingHeap; }
		RenderPassDescriptorHeap& GetRenderPassHeap() { return *m_RenderPassHeap; }
		StagingDescriptorHeap& GetDepthStencilHeap() { return *m_DepthStencilHeap; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		std::unique_ptr<StagingDescriptorHeap> m_StagingHeap;
		std::unique_ptr<RenderPassDescriptorHeap> m_RenderPassHeap;
		std::unique_ptr<StagingDescriptorHeap> m_DepthStencilHeap;
	};
}

