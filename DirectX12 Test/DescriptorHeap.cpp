#include "DescriptorHeap.h"
#include "EngineUtils.h"

namespace DX12Engine
{
	DescriptorHeap::DescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool isReferencedByShader)
	{
		m_HeapType = heapType;
		m_MaxDescriptors = numDescriptors;
		m_IsReferencedByShader = isReferencedByShader;

		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = numDescriptors;
		heapDesc.Type = heapType;
		heapDesc.Flags = isReferencedByShader ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;

		EngineUtils::ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_DescriptorHeap)));

		m_DescriptorHeapCPUStart = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		if (isReferencedByShader)
			m_DescriptorHeapGPUStart = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		m_DescriptorSize = device->GetDescriptorHandleIncrementSize(heapType);
	}

	DescriptorHeap::~DescriptorHeap()
	{
		m_DescriptorHeap->Release();
		m_DescriptorHeap = nullptr;
	}
}