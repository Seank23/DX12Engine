#pragma once
#include "d3dx12.h"

namespace DX12Engine
{
	class GPUResource
	{
	public:
		GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState);
		virtual ~GPUResource();

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_GPUAddress; }
		D3D12_RESOURCE_STATES GetUsageState() const { return m_UsageState; }
		void SetUsageState(D3D12_RESOURCE_STATES usageState) { m_UsageState = usageState; }

		bool GetIsReady() const { return m_IsReady; }
		void SetIsReady(bool isReady) { m_IsReady = isReady; }

		ID3D12Resource* GetResource() const { return m_Resource; }

	protected:
		ID3D12Resource* m_Resource;
		D3D12_GPU_VIRTUAL_ADDRESS m_GPUAddress;
		D3D12_RESOURCE_STATES m_UsageState;
		bool m_IsReady;
	};
}

