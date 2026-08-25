#include "GPUUploader.h"
#include "../Utils/Constants.h"
#include "./RenderContext.h"
#include "../Utils/EngineUtils.h"

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
		m_GraphicsUploadList.Init(context.GetDevice().Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, FRAMES_IN_FLIGHT * 4);
		m_CopyUploadList.Init(context.GetDevice().Get(), D3D12_COMMAND_LIST_TYPE_COPY, FRAMES_IN_FLIGHT * 4);
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
			UpdateSubresources(m_CopyUploadList.GetCommandList(), texture->GetResource(), texture->m_UploadResource, 0, 0, static_cast<UINT>(texture->m_Data.size()), texture->m_Data.data());
			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(texture->m_MainResource,
																D3D12_RESOURCE_STATE_COPY_DEST,
																D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_GraphicsUploadList.GetCommandList()->ResourceBarrier(1, &barrier);

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

		UpdateSubresources(m_CopyUploadList.GetCommandList(), resourceWrapper.GPUResource->GetResource(), resourceWrapper.UploadResource, 0, 0, 1, &resourceWrapper.Data);
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resourceWrapper.GPUResource->GetResource(),
															resourceWrapper.GPUResource->GetUsageState(),
															resourceWrapper.UploadState);
		m_GraphicsUploadList.GetCommandList()->ResourceBarrier(1, &barrier);
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

		UINT copyFenceVal = m_CopyUploadList.Submit(copyQueue);
		graphicsQueue.InsertWaitForQueueFence(&copyQueue, copyFenceVal);
		UINT graphicsFenceVal = m_GraphicsUploadList.Submit(graphicsQueue);

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

		m_CopyUploadList.Begin(m_QueueManager.GetCopyQueue());
		m_GraphicsUploadList.Begin(m_QueueManager.GetGraphicsQueue());
		m_UploadListsRecording = true;
	}

	void UploadCommandList::Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType, UINT slotCount)
	{
		m_Slots.resize(slotCount);
		for (Slot& slot : m_Slots)
			EngineUtils::ThrowIfFailed(device->CreateCommandAllocator(commandType, IID_PPV_ARGS(&slot.Allocator)));
		EngineUtils::ThrowIfFailed(device->CreateCommandList(0, commandType, m_Slots[0].Allocator.Get(), nullptr, IID_PPV_ARGS(&m_CommandList)));
		EngineUtils::ThrowIfFailed(m_CommandList->Close());
	}

	void UploadCommandList::Begin(CommandQueue& queue)
	{
		const size_t oldest = (m_CurrentSlot + 1) % m_Slots.size();
		size_t selected = oldest;
		bool found = false;
		for (size_t i = 0; i < m_Slots.size() && !found; i++)
		{
			const size_t candidate = (oldest + i) % m_Slots.size();
			if (m_Slots[candidate].FenceValue == 0 || queue.IsFenceComplete(static_cast<UINT>(m_Slots[candidate].FenceValue)))
			{
				selected = candidate;
				found = true;
			}
		}
		if (!found)
			queue.WaitForFenceCPUBlocking(static_cast<UINT>(m_Slots[oldest].FenceValue));

		m_CurrentSlot = selected;
		m_Slots[m_CurrentSlot].FenceValue = 0;
		EngineUtils::ThrowIfFailed(m_Slots[m_CurrentSlot].Allocator->Reset());
		EngineUtils::ThrowIfFailed(m_CommandList->Reset(m_Slots[m_CurrentSlot].Allocator.Get(), nullptr));
	}

	UINT UploadCommandList::Submit(CommandQueue& queue)
	{
		const UINT fenceValue = queue.SubmitCommandList(m_CommandList.Get());
		m_Slots[m_CurrentSlot].FenceValue = fenceValue;
		return fenceValue;
	}
}
