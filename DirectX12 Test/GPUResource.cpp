#include "GPUResource.h"

namespace DX12Engine
{
	GPUResource::GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState)
		: m_Resource(resource), m_UsageState(usageState), m_IsReady(false)
	{
		m_GPUAddress = m_Resource->GetGPUVirtualAddress();
	}

	GPUResource::~GPUResource()
	{
		m_Resource->Release();
	}
}
