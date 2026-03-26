#include "DescriptorHeapManager.h"

namespace DX12Engine
{
	DescriptorHeapManager::DescriptorHeapManager(Microsoft::WRL::ComPtr<ID3D12Device> device)
	{
		m_Device = device;
		m_StagingHeap = std::make_unique<StagingDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRV_PERSISTENT_CAPACITY);
		m_RenderPassHeap = std::make_unique<RenderPassDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SRV_TRANSIENT_CAPACITY_FRAME);
		m_DepthStencilHeap = std::make_unique<StagingDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, DSV_CAPACITY);
		m_RenderTargetHeap = std::make_unique<StagingDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, RTV_CAPACITY);
	}

	DescriptorHeapManager::~DescriptorHeapManager()
	{
	}

	DescriptorHeapStats DescriptorHeapManager::GetStats() const
	{
		const DescriptorHeapStats& persistentStats = m_StagingHeap->GetStats();
		const DescriptorHeapStats& renderPassStats = m_RenderPassHeap->GetStats();

		DescriptorHeapStats stats{};
		stats.persistentUsed = persistentStats.persistentUsed;
		stats.persistentCapacity = persistentStats.persistentCapacity;
		stats.transientUsedThisFrame = renderPassStats.transientUsedThisFrame;
		stats.transientPeakThisFrame = renderPassStats.transientPeakThisFrame;
		stats.transientCapacityPerFrame = renderPassStats.transientCapacityPerFrame;
		stats.allocationFailures = renderPassStats.allocationFailures;
		return stats;
	}
}