#pragma once
#include "../Queues/CommandQueueManager.h"
#include "../Resources/Texture.h"
#include "../Resources/UploadResourceWrapper.h"

namespace DX12Engine
{
	class RenderContext;
	class RenderPassDescriptorHeap;

	class GPUUploader
	{
	public:
		GPUUploader(RenderContext& context);
		~GPUUploader();

		void UploadTextureBatch(std::vector<Texture*> textures);
		void UploadResource(UploadResourceWrapper resourceWrapper);

		void ExecuteUpload();
		bool UploadAllPending();

	private:

		CommandQueueManager& m_QueueManager;
		RenderContext& m_RenderContext;

		ID3D12GraphicsCommandList* m_GraphicsCommandList;
		ID3D12GraphicsCommandList* m_CopyCommandList;

		RenderPassDescriptorHeap& m_RenderHeap;

		int m_UploadCount = 0;
	};
}

