#include "GPUUploader.h"
#include "../Utils/Constants.h"
#include "./RenderContext.h"
#include "Heaps/RenderPassDescriptorHeap.h"

namespace DX12Engine
{
	namespace
	{
		void SafeRelease(ID3D12Resource*& resource)
		{
			if (!resource)
				return;

			resource->Release();
			resource = nullptr;
		}
	}

	GPUUploader::GPUUploader(RenderContext& context)
		: m_RenderContext(context), m_QueueManager(context.GetQueueManager()), m_RenderHeap(context.GetHeapManager().GetRenderPassHeap())
	{
		m_GraphicsCommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();
		m_CopyCommandList = m_QueueManager.GetCopyQueue().GetCommandList();
	}

	GPUUploader::~GPUUploader()
	{
		ReleasePendingUploadResources();
		ReleasePendingReferencedResources();
	}

	void GPUUploader::UploadTextureBatch(std::vector<Texture*> textures)
	{
		std::vector<Texture*> texturesToUpload;
		texturesToUpload.reserve(textures.size());
		for (Texture* texture : textures)
		{
			if (!texture || texture->GetIsReady() || !texture->m_UploadResource || texture->m_Data.empty())
				continue;
			texturesToUpload.push_back(texture);
		}

		if (texturesToUpload.empty())
			return;

		EnsureUploadListsRecording();

		DescriptorHeapHandle renderBlockStart = m_RenderHeap.GetHeapHandleBlock(texturesToUpload.size());
		D3D12_CPU_DESCRIPTOR_HANDLE currentCPUHandle = renderBlockStart.GetCPUHandle();
		D3D12_GPU_DESCRIPTOR_HANDLE currentGPUHandle = renderBlockStart.GetGPUHandle();
		UINT descriptorSize = m_RenderHeap.GetDescriptorSize();	
		for (Texture* texture : texturesToUpload)
		{
			UpdateSubresources(m_CopyCommandList, texture->GetResource(), texture->m_UploadResource, 0, 0, static_cast<UINT>(texture->m_Data.size()), texture->m_Data.data());
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->m_MainResource,
				D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_GraphicsCommandList->ResourceBarrier(1, &barrier);

			m_RenderContext.GetDevice()->CopyDescriptorsSimple(1, currentCPUHandle, texture->GetDescriptor()->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			texture->GetDescriptor()->SetGPUHandle(currentGPUHandle);

			texture->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			texture->SetIsReady(true);

			currentCPUHandle.ptr += descriptorSize;
			currentGPUHandle.ptr += descriptorSize;
		}
		ExecuteUpload();

		for (Texture* texture : texturesToUpload)
		{
			SafeRelease(texture->m_UploadResource);
			texture->m_Data.clear();
			texture->m_Data.shrink_to_fit();
			texture->m_IsUploaded = true;
		}
	}

	void GPUUploader::UploadResource(UploadResourceWrapper resourceWrapper)
	{
		EnsureUploadListsRecording();

		ID3D12Resource* referencedResource = resourceWrapper.GPUResource ? resourceWrapper.GPUResource->GetResource() : nullptr;
		if (referencedResource)
		{
			referencedResource->AddRef();
			m_PendingReferencedResources.push_back(referencedResource);
		}

		UpdateSubresources(m_CopyCommandList, resourceWrapper.GPUResource->GetResource(), resourceWrapper.UploadResource, 0, 0, 1, &resourceWrapper.Data);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceWrapper.GPUResource->GetResource(),
			resourceWrapper.GPUResource->GetUsageState(), resourceWrapper.UploadState);
		m_GraphicsCommandList->ResourceBarrier(1, &barrier);
		resourceWrapper.GPUResource->SetUsageState(resourceWrapper.UploadState);
		resourceWrapper.GPUResource->SetIsReady(true);
		if (resourceWrapper.UploadResource)
			m_PendingUploadResources.push_back(resourceWrapper.UploadResource);
		if (++m_UploadCount >= MAX_UPLOAD_BATCH_SIZE)
		{
			ExecuteUpload();
			m_UploadCount = 0;
		}
	}

	void GPUUploader::ExecuteUpload()
	{
		if (!m_UploadListsRecording)
			return;

		UINT copyFenceVal = m_QueueManager.GetCopyQueue().ExecuteCommandList();
		m_QueueManager.GetCopyQueue().WaitForFenceCPUBlocking(copyFenceVal);

		UINT graphicsFenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(graphicsFenceVal);

		m_UploadListsRecording = false;
		ReleasePendingUploadResources();
		ReleasePendingReferencedResources();
	}

	bool GPUUploader::UploadAllPending()
	{
		if (m_UploadCount > 0)
		{
			ExecuteUpload();
			m_UploadCount = 0;
			return true;
		}
		return false;
	}

	void GPUUploader::ReleasePendingUploadResources()
	{
		for (ID3D12Resource* uploadResource : m_PendingUploadResources)
			SafeRelease(uploadResource);
		m_PendingUploadResources.clear();
	}

	void GPUUploader::ReleasePendingReferencedResources()
	{
		for (ID3D12Resource* referencedResource : m_PendingReferencedResources)
			SafeRelease(referencedResource);
		m_PendingReferencedResources.clear();
	}

	void GPUUploader::EnsureUploadListsRecording()
	{
		if (m_UploadListsRecording)
			return;

		m_QueueManager.GetCopyQueue().ResetCommandAllocatorAndList();
		m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();
		m_UploadListsRecording = true;
	}
}
