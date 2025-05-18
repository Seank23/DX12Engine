#pragma once
#include "d3dx12.h"
#include "../Rendering/Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class GPUResource
	{
	public:
		GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle descriptor, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc);
		GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle descriptor);
		GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc);
		GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		virtual ~GPUResource();

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_GPUAddress; }
		D3D12_RESOURCE_STATES GetUsageState() const { return m_UsageState; }
		void SetUsageState(D3D12_RESOURCE_STATES usageState) { m_UsageState = usageState; }

		bool GetIsReady() const { return m_IsReady; }
		void SetIsReady(bool isReady) { m_IsReady = isReady; }

		ID3D12Resource* GetResource() const { return m_Resource; }
		DescriptorHeapHandle* GetDescriptor() const { return m_Descriptor.get(); }
		void SetDescriptor(DescriptorHeapHandle& descriptor) { m_Descriptor = std::make_unique<DescriptorHeapHandle>(descriptor); }

		D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc() const { return m_SRVDesc; }

	protected:
		ID3D12Resource* m_Resource;
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress;
		D3D12_RESOURCE_STATES m_UsageState;
		std::unique_ptr<DescriptorHeapHandle> m_Descriptor;
		D3D12_SHADER_RESOURCE_VIEW_DESC m_SRVDesc;
		bool m_IsReady;
	};
}

