#pragma once
#include "../Resources/GPUResource.h"
#include "../Heaps/DescriptorHeapHandle.h"

namespace DX12Engine
{
	class ConstantBuffer : public GPUResource
	{
	public:
		ConstantBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES usageState, UINT bufferSize, DescriptorHeapHandle cbvHandle);
		~ConstantBuffer() override;

		void Update(void* data, UINT size);
		DescriptorHeapHandle GetConstantBufferViewHandle() const { return m_CBVHandle; }	

	private:
		void* m_MappedBuffer;
		UINT m_BufferSize;
		DescriptorHeapHandle m_CBVHandle;
	};
}	
