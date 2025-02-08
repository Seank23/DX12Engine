#include "GPUUploader.h"

namespace DX12Engine
{
	GPUUploader::GPUUploader(std::shared_ptr<RenderContext> context)
		: m_QueueManager(context->GetQueueManager()), m_RenderHeap(context->GetHeapManager().GetRenderPassHeap())
	{
		m_RenderContext = context;
		m_GraphicsCommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();
		m_CopyCommandList = m_QueueManager.GetCopyQueue().GetCommandList();
	}

	GPUUploader::~GPUUploader()
	{
	}

	void GPUUploader::UploadTextureBatch(std::vector<Texture*> textures)
	{
		DescriptorHeapHandle renderBlockStart = m_RenderHeap.GetHeapHandleBlock(textures.size());
		D3D12_CPU_DESCRIPTOR_HANDLE currentCPUHandle = renderBlockStart.GetCPUHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE currentGPUHandle = renderBlockStart.GetGPUHandle();
		UINT descriptorSize = m_RenderHeap.GetDescriptorSize();	
		for (Texture* texture : textures)
		{
			UpdateSubresources(m_CopyCommandList, texture->m_MainResource, texture->m_UploadResource, 0, 0, 1, &texture->m_Data);
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->m_MainResource,
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_GraphicsCommandList->ResourceBarrier(1, &barrier);

			m_RenderContext->GetDevice()->CopyDescriptorsSimple(1, currentCPUHandle, texture->GetDescriptor()->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			texture->GetDescriptor()->SetGPUHandle(currentGPUHandle);

			texture->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			texture->SetIsReady(true);

			currentCPUHandle.ptr += descriptorSize;
			currentGPUHandle.ptr += descriptorSize;
		}
		UINT copyFenceVal = m_QueueManager.GetCopyQueue().ExecuteCommandList();
		UINT graphicsFenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.GetCopyQueue().WaitForFenceCPUBlocking(copyFenceVal);
		m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(graphicsFenceVal);
	}
}
