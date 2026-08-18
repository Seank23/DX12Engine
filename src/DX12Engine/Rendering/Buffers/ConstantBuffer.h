#pragma once
#include "../../Resources/GPUResource.h"
#include "../Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class ConstantBuffer : public GPUResource
	{
	public:
		ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT alignedSizePerFrame);
		~ConstantBuffer() override;

		void Update(void* data, UINT size);
		void UpdateAllFrames(void* data, UINT size);

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const override;

		static void SetFrameSlot(UINT frameSlot) { s_FrameSlot = frameSlot; }

	private:
		uint8_t* m_MappedBuffer;
		UINT m_AlignedSizePerFrame;
		static UINT s_FrameSlot;
	};
}
