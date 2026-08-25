#pragma once
#include "Queues/CommandQueueManager.h"
#include "../Resources/Texture.h"
#include "../Resources/UploadResourceWrapper.h"
#include <vector>

namespace DX12Engine
{
	class RenderContext;

	struct PendingRelease
	{
		ID3D12Resource* Resource;
		UINT64 FenceValue;
	};

	class UploadCommandList
	{
	public:
		void Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE commandType, UINT slotCount);
		void Begin(CommandQueue& queue);
		UINT Submit(CommandQueue& queue);
		ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList.Get(); }

	private:
		struct Slot
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> Allocator;
			UINT64 FenceValue = 0;
		};
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_CommandList;
		std::vector<Slot> m_Slots;
		UINT m_CurrentSlot = 0;
	};

	class GPUUploader
	{
	public:
		GPUUploader(RenderContext& context);
		~GPUUploader();
		GPUUploader(const GPUUploader&) = delete;
		GPUUploader& operator=(const GPUUploader&) = delete;
		GPUUploader(GPUUploader&&) = delete;
		GPUUploader& operator=(GPUUploader&&) = delete;

		void UploadTextureBatch(std::vector<Texture*> textures);
		void UploadResource(UploadResourceWrapper resourceWrapper);

		void ExecuteUpload();
		bool UploadAllPending();

	private:
		void EnsureUploadListsRecording();
		void QueuePendingReleases(UINT fenceValue);
		void ProcessRetiredResources();

		CommandQueueManager& m_QueueManager;
		RenderContext& m_RenderContext;

		UploadCommandList m_GraphicsUploadList, m_CopyUploadList;

		int m_UploadCount = 0;
		bool m_UploadListsRecording = false;
		std::vector<ID3D12Resource*> m_PendingUploadResources;
		std::vector<ID3D12Resource*> m_PendingReferencedResources;
		std::vector<PendingRelease> m_RetiringResources;
	};
}