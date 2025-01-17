#include "DescriptorHeapManager.h"

namespace DX12Engine
{
	static DescriptorHeapManager* s_Instance = nullptr;

	DescriptorHeapManager::DescriptorHeapManager()
	{
	}

	DescriptorHeapManager::~DescriptorHeapManager()
	{
		delete m_StagingHeap;
		delete m_RenderPassHeap;
	}

	DescriptorHeapManager& DescriptorHeapManager::GetInstance()
	{
		if (!s_Instance)
			s_Instance = new DescriptorHeapManager();
		return *s_Instance;
	}

	void DescriptorHeapManager::Init(Microsoft::WRL::ComPtr<ID3D12Device> device)
	{
		m_Device = device;
		m_StagingHeap = new StagingDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);
		m_RenderPassHeap = new RenderPassDescriptorHeap(device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16);
	}

	void DescriptorHeapManager::Shutdown()
	{
		delete s_Instance;
		s_Instance = nullptr;
	}
}
