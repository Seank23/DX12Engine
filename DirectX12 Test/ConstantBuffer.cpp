#include "ConstantBuffer.h"
#include "EngineUtils.h"

namespace DX12Engine
{
	ConstantBuffer::ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT bufferSize)
		: GPUResource(resource, usageState)
	{
		m_GPUAddress = resource->GetGPUVirtualAddress();
		m_BufferSize = bufferSize;

		m_MappedBuffer = nullptr;
		m_Resource->Map(0, nullptr, reinterpret_cast<void**>(&m_MappedBuffer));
	}

	ConstantBuffer::~ConstantBuffer()
	{
		m_Resource->Unmap(0, nullptr);
		m_MappedBuffer = nullptr;
	}

	void ConstantBuffer::Update(void* data, UINT size)
	{
		EngineUtils::Assert(size <= m_BufferSize);
		memcpy(m_MappedBuffer, data, size);
	}
}