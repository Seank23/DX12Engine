#pragma once
#include <d3dx12.h>
#include "../RenderContext.h"
#include "../Queues/CommandQueueManager.h"
#include "../RootSignatureBuilder.h"

namespace DX12Engine
{
	enum class RenderTargetType
	{
		Albedo,
		WorldNormal,
		ObjectNormal,
		Material,
		Position,
		Depth,
		Composite
	};

	class RenderComponent;
	class GPUResource;
	class RenderTexture;
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

		void AddInputResources(std::vector<GPUResource*> resources) 
		{ 
			m_InputResources.insert(m_InputResources.end(), resources.begin(), resources.end());
		}
		void AddInputResources(std::vector<std::shared_ptr<GPUResource>> resources)
		{
			m_InputResources.insert(m_InputResources.end(), resources.begin(), resources.end());
		}
		void SetRenderObjects(std::vector<RenderComponent*> renderObjects) { m_RenderObjects = renderObjects; }
		virtual RenderTexture* GetRenderTarget(RenderTargetType type) = 0;

		void AddDescriptorTableConfig(DescriptorTableConfig config) { m_DescriptorTableConfigs.push_back(config); }

	protected:
		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList& m_CommandList;

		std::vector<std::shared_ptr<GPUResource>> m_InputResources;
		std::vector<DescriptorTableConfig> m_DescriptorTableConfigs;
		std::vector<std::unique_ptr<RenderTexture>> m_RenderTargets;
		std::vector<RenderComponent*> m_RenderObjects;
	};
}
