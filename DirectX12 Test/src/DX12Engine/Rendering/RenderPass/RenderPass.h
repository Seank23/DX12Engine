#pragma once
#include <d3dx12.h>
#include "../RenderContext.h"
#include "../../Queues/CommandQueueManager.h"

namespace DX12Engine
{
	class RenderObject;
	class GPUResource;

	class RenderPass
	{
	public:
		RenderPass(RenderContext& context)
			: m_RenderContext(context), m_QueueManager(context.GetQueueManager()), m_CommandList(*m_QueueManager.GetGraphicsQueue().GetCommandList())
			{}
		~RenderPass() = default;
		virtual void Init() = 0;
		virtual void Execute() = 0;

		void SetInputResources(std::vector<GPUResource*> resources) { m_InputResources = resources; }
		void SetRenderObjects(std::vector<RenderObject*> renderObjects) { m_RenderObjects = renderObjects; }

	protected:
		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList& m_CommandList;

		std::vector<GPUResource*> m_InputResources;
		std::vector<std::unique_ptr<GPUResource>> m_RenderTargets;
		std::vector<RenderObject*> m_RenderObjects;
	};
}
