#pragma once
#include "RenderContext.h"
#include "../Resources/Texture.h"
#include "../Queues/CommandQueueManager.h"

namespace DX12Engine
{
	class GPUUploader
	{
	public:
		GPUUploader(std::shared_ptr<RenderContext> context);
		~GPUUploader();

		void UploadTextureBatch(std::vector<Texture*> textures);

	private:
		CommandQueueManager& m_QueueManager;
		std::shared_ptr<RenderContext> m_RenderContext;

		ID3D12GraphicsCommandList* m_GraphicsCommandList;
		ID3D12GraphicsCommandList* m_CopyCommandList;

		RenderPassDescriptorHeap& m_RenderHeap;
	};
}

