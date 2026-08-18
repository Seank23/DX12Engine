#include "ConstantBuffer.h"
#include "../../Utils/EngineUtils.h"
#include "../../Utils/Constants.h"

namespace DX12Engine
{
	UINT ConstantBuffer::s_FrameSlot = 0;

	ConstantBuffer::ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT alignedSizePerFrame)
		: GPUResource(resource, usageState)
	{
		m_AlignedSizePerFrame = alignedSizePerFrame;

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
		EngineUtils::Assert(size <= m_AlignedSizePerFrame);
		memcpy(m_MappedBuffer + s_FrameSlot * m_AlignedSizePerFrame, data, size);
	}

	void ConstantBuffer::UpdateAllFrames(void* data, UINT size)
	{
		EngineUtils::Assert(size <= m_AlignedSizePerFrame);
		for (UINT frame = 0; frame < FRAMES_IN_FLIGHT; frame++)
			memcpy(m_MappedBuffer + frame * m_AlignedSizePerFrame, data, size);
	}

	D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::GetGPUAddress() const
	{
		return m_GPUAddress + s_FrameSlot * m_AlignedSizePerFrame;
	}
}