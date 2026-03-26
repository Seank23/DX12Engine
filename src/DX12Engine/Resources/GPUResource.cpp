#include "GPUResource.h"

namespace DX12Engine
{
	namespace
	{
		// GetGPUVirtualAddress is only valid for buffer resources.
		// Calling it on a texture returns 0 and triggers a D3D12 debug warning.
		D3D12_GPU_VIRTUAL_ADDRESS SafeGetGPUAddress(ID3D12Resource* resource)
		{
			if (!resource)
				return 0;
			D3D12_RESOURCE_DESC desc = resource->GetDesc();
			if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
				return resource->GetGPUVirtualAddress();
			return 0;
		}
	}

	GPUResource::GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle descriptor, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
		: m_Resource(resource), m_UsageState(usageState), m_IsReady(false), m_TransientDescriptor(nullptr), m_PersistentDescriptor(nullptr)
	{
		m_GPUAddress = SafeGetGPUAddress(resource);
		m_TransientDescriptor = std::make_unique<DescriptorHeapHandle>(descriptor);
		m_PersistentDescriptor = std::make_unique<DescriptorHeapHandle>(descriptor);
		m_SRVDesc = srvDesc;
	}

	GPUResource::GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, DescriptorHeapHandle descriptor)
		: m_Resource(resource), m_UsageState(usageState), m_IsReady(false), m_TransientDescriptor(nullptr), m_PersistentDescriptor(nullptr)
	{
		m_GPUAddress = SafeGetGPUAddress(resource);
		m_TransientDescriptor = std::make_unique<DescriptorHeapHandle>(descriptor);
		m_PersistentDescriptor = std::make_unique<DescriptorHeapHandle>(descriptor);
	}

	GPUResource::GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc)
		: m_Resource(resource), m_UsageState(usageState), m_IsReady(false), m_TransientDescriptor(nullptr), m_PersistentDescriptor(nullptr)
	{
		m_GPUAddress = SafeGetGPUAddress(resource);
		m_SRVDesc = srvDesc;
	}

	GPUResource::GPUResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState)
		: m_Resource(resource), m_UsageState(usageState), m_IsReady(false), m_TransientDescriptor(nullptr), m_PersistentDescriptor(nullptr)
	{
		m_GPUAddress = SafeGetGPUAddress(resource);
	}

	GPUResource::~GPUResource()
	{
		m_Resource->Release();
	}
}

