#pragma once
#include "../../Utils/Constants.h"

#include "d3dx12.h"

namespace DX12Engine
{
	class FrameConstantAllocator
	{
	public:
		FrameConstantAllocator(ID3D12Device* device, UINT bytesPerFrame);
		~FrameConstantAllocator();
		FrameConstantAllocator(const FrameConstantAllocator&) = delete;
		FrameConstantAllocator& operator=(const FrameConstantAllocator&) = delete;

		void BeginFrame(UINT slot);
		D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* data, UINT size);

		UINT GetPeakBytesUsed() const { return m_PeakBytesUsed; }

		private:
			struct FrameSlot
			{
				Microsoft::WRL::ComPtr<ID3D12Resource> Buffer;
				uint8_t* MappedData = nullptr;
				D3D12_GPU_VIRTUAL_ADDRESS BaseAddress = 0;
				UINT Cursor = 0;
			};

			FrameSlot m_FrameSlots[FRAMES_IN_FLIGHT];
			UINT m_CurrentSlot = 0;
			UINT m_BytesPerFrame = 0;
			UINT m_PeakBytesUsed = 0;
	};
}