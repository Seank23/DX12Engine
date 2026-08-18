#define NOMINMAX
#include "FrameConstantAllocator.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	FrameConstantAllocator::FrameConstantAllocator(ID3D12Device* device, UINT bytesPerFrame)
		: m_BytesPerFrame(bytesPerFrame), m_CurrentSlot(0), m_PeakBytesUsed(0)
	{
		D3D12_RESOURCE_DESC constantBufferDesc;
		constantBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		constantBufferDesc.Alignment = 0;
		constantBufferDesc.Width = bytesPerFrame;
		constantBufferDesc.Height = 1;
		constantBufferDesc.DepthOrArraySize = 1;
		constantBufferDesc.MipLevels = 1;
		constantBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		constantBufferDesc.SampleDesc.Count = 1;
		constantBufferDesc.SampleDesc.Quality = 0;
		constantBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		constantBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES uploadHeapProperties;
		uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapProperties.CreationNodeMask = 0;
		uploadHeapProperties.VisibleNodeMask = 0;

		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			EngineUtils::ThrowIfFailed(device->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&constantBufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_FrameSlots[i].Buffer)));
			m_FrameSlots[i].Cursor = 0;
			EngineUtils::ThrowIfFailed(m_FrameSlots[i].Buffer->Map(0, nullptr, reinterpret_cast<void**>(&m_FrameSlots[i].MappedData)));
			m_FrameSlots[i].BaseAddress = m_FrameSlots[i].Buffer->GetGPUVirtualAddress();
		}
	}

	FrameConstantAllocator::~FrameConstantAllocator()
	{
		for (int i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			if (m_FrameSlots[i].Buffer)
			{
				m_FrameSlots[i].Buffer->Unmap(0, nullptr);
				m_FrameSlots[i].MappedData = nullptr;
			}
		}
	}

	void FrameConstantAllocator::BeginFrame(UINT slot)
	{
		m_CurrentSlot = slot;
		m_PeakBytesUsed = std::max(m_PeakBytesUsed, m_FrameSlots[slot].Cursor);
		m_FrameSlots[slot].Cursor = 0;
	}

	D3D12_GPU_VIRTUAL_ADDRESS FrameConstantAllocator::Allocate(const void* data, UINT size)
	{
		FrameSlot& frameSlot = m_FrameSlots[m_CurrentSlot];
		UINT alignedSize = EngineUtils::AlignUINT(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		if (frameSlot.Cursor + alignedSize > m_BytesPerFrame)
			throw std::runtime_error("FrameConstantAllocator: Out of memory for this frame.");

		UINT offset = frameSlot.Cursor;
		memcpy(frameSlot.MappedData + offset, data, size);
		frameSlot.Cursor += alignedSize;

		return frameSlot.BaseAddress + offset;
	}
}