#pragma once
#include "StagingDescriptorHeap.h"
#include "RenderPassDescriptorHeap.h"

namespace DX12Engine
{
	class DescriptorHeapManager
	{
	public:
		static DescriptorHeapManager& GetInstance();
		void Init(Microsoft::WRL::ComPtr<ID3D12Device> device);
		static void Shutdown();

		DescriptorHeapManager(const DescriptorHeapManager&) = delete;
		DescriptorHeapManager& operator=(const DescriptorHeapManager&) = delete;

	private:
		DescriptorHeapManager();
		~DescriptorHeapManager();

	public:
		DescriptorHeapHandle GetNewSRVDescriptorHeapHandle() { return m_StagingHeap->GetNewHeapHandle(); }
		DescriptorHeapHandle GetRenderHeapHandleBlock(UINT count) { return m_RenderPassHeap->GetHeapHandleBlock(count); }

		StagingDescriptorHeap& GetStagingHeap() { return *m_StagingHeap; }
		RenderPassDescriptorHeap& GetRenderPassHeap() { return *m_RenderPassHeap; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;
		std::unique_ptr<StagingDescriptorHeap> m_StagingHeap;
		std::unique_ptr<RenderPassDescriptorHeap> m_RenderPassHeap;
	};
}

