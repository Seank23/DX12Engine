#pragma once
#include "GPUResource.h"

namespace DX12Engine
{
	class ConstantBuffer : GPUResource
	{
	public:
		ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT bufferSize);
		~ConstantBuffer();

		void Update(void* data, UINT size);

		D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() { return GetGPUAddress(); }

	private:
		void* m_MappedBuffer;
		UINT m_BufferSize;
	};
}	
