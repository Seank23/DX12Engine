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

		ID3D12GraphicsCommandList* m_GraphicsCommandList;
		ID3D12GraphicsCommandList* m_CopyCommandList;

		int m_UploadCount = 0;
		bool m_UploadListsRecording = false;
		std::vector<ID3D12Resource*> m_PendingUploadResources;
		std::vector<ID3D12Resource*> m_PendingReferencedResources;
		std::vector<PendingRelease> m_RetiringResources;
	};
}