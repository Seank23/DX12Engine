#pragma once
#include "Queues/CommandQueueManager.h"
#include "../Resources/Texture.h"
#include "../Resources/UploadResourceWrapper.h"
#include <vector>

namespace DX12Engine
{
	class RenderContext;
	class RenderPassDescriptorHeap;

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
		void ReleasePendingUploadResources();
		void ReleasePendingReferencedResources();

		CommandQueueManager& m_QueueManager;
		RenderContext& m_RenderContext;

		ID3D12GraphicsCommandList* m_GraphicsCommandList;
		ID3D12GraphicsCommandList* m_CopyCommandList;

		RenderPassDescriptorHeap& m_RenderHeap;

		int m_UploadCount = 0;
		bool m_UploadListsRecording = false;
		std::vector<ID3D12Resource*> m_PendingUploadResources;
		std::vector<ID3D12Resource*> m_PendingReferencedResources;
	};
}

