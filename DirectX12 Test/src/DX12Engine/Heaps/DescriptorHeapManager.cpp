#include "DescriptorHeapManager.h"

namespace DX12Engine
{
	DescriptorHeapManager::DescriptorHeapManager(Microsoft::WRL::ComPtr<ID3D12Device> device)
	{
		m_Device = device;
		m_StagingHeap = std::make_unique<StagingDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32);
		m_RenderPassHeap = std::make_unique<RenderPassDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 32);
		m_DepthStencilHeap = std::make_unique<StagingDescriptorHeap>(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 32);
	}

	DescriptorHeapManager::~DescriptorHeapManager()
	{
	}
}
