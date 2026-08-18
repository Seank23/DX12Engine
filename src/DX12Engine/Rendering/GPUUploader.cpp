#include "GPUUploader.h"
#include "../Utils/Constants.h"
#include "./RenderContext.h"

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
		: m_RenderContext(context), m_QueueManager(context.GetQueueManager())
	{
		m_GraphicsCommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();
		m_CopyCommandList = m_QueueManager.GetCopyQueue().GetCommandList();
	}

	GPUUploader::~GPUUploader()
	{
		m_QueueManager.GetGraphicsQueue().WaitForIdle();
		ProcessRetiredResources();
		for (ID3D12Resource* uploadResource : m_PendingUploadResources)
			SafeRelease(uploadResource);
		for (ID3D12Resource* referencedResource : m_PendingReferencedResources)
			SafeRelease(referencedResource);
		m_PendingUploadResources.clear();
		m_PendingReferencedResources.clear();
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

		ID3D12Device* device = m_RenderContext.GetDevice().Get();
		for (Texture* texture : texturesToUpload)
		{
			UpdateSubresources(m_CopyCommandList, texture->GetResource(), texture->m_UploadResource, 0, 0, static_cast<UINT>(texture->m_Data.size()), texture->m_Data.data());
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->m_MainResource,
																D3D12_RESOURCE_STATE_COPY_DEST,
																D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_GraphicsCommandList->ResourceBarrier(1, &barrier);

			// Write the SRV into the persistent non-shader-visible staging slot so it
			// can be used as a CopyDescriptorsSimple source every frame.
			DescriptorHeapHandle* persistentHandle = texture->GetPersistentDescriptor();
			if (persistentHandle && persistentHandle->IsValid())
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = texture->GetSRVDesc();
				device->CreateShaderResourceView(texture->GetResource(), &srvDesc, persistentHandle->GetCPUHandle());
			}

			texture->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			texture->SetIsReady(true);
			if (texture->m_UploadResource)
			{
				m_PendingUploadResources.push_back(texture->m_UploadResource);
				texture->m_UploadResource = nullptr; // ownership moves to the retire list
			}
		}
		ExecuteUpload();

		for (Texture* texture : texturesToUpload)
		{
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
															resourceWrapper.GPUResource->GetUsageState(),
															resourceWrapper.UploadState);
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
		auto& copyQueue = m_QueueManager.GetCopyQueue();
		auto& graphicsQueue = m_QueueManager.GetGraphicsQueue();
		UINT copyFenceVal = copyQueue.ExecuteCommandList();
		graphicsQueue.InsertWaitForQueueFence(&copyQueue, copyFenceVal);
		UINT graphicsFenceVal = graphicsQueue.ExecuteCommandList();

		m_UploadListsRecording = false;
		QueuePendingReleases(graphicsFenceVal);
		ProcessRetiredResources();
	}

	bool GPUUploader::UploadAllPending()
	{
		if (m_UploadCount > 0)
		{
			ExecuteUpload();
			m_UploadCount = 0;
			return true;
		}
		else
		{
			ProcessRetiredResources();
		}
		return false;
	}

	void GPUUploader::QueuePendingReleases(UINT fenceValue)
	{
		for (ID3D12Resource* uploadResource : m_PendingUploadResources)
			m_RetiringResources.push_back({ uploadResource, fenceValue });
		for (ID3D12Resource* referencedResource : m_PendingReferencedResources)
			m_RetiringResources.push_back({ referencedResource, fenceValue });
		m_PendingUploadResources.clear();
		m_PendingReferencedResources.clear();
	}

	void GPUUploader::ProcessRetiredResources()
	{
		UINT currentFenceValue = m_QueueManager.GetGraphicsQueue().PollCurrentFenceValue();
		size_t keep = 0;
		for (auto& resource : m_RetiringResources)
		{
			if (!resource.Resource || currentFenceValue >= resource.FenceValue)
			{
				SafeRelease(resource.Resource);
				continue;
			}
			m_RetiringResources[keep++] = resource;
		}
		m_RetiringResources.resize(keep);
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
