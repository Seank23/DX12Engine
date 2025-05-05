#pragma once
#include <d3dx12.h>
#include "../RenderContext.h"
#include "../../Queues/CommandQueueManager.h"
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

	class RenderObject;
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
			AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, m_InputResourceCount });
			m_InputResourceCount += resources.size();
		}
		void SetRenderObjects(std::vector<RenderObject*> renderObjects) { m_RenderObjects = renderObjects; }
		virtual RenderTexture* GetRenderTarget(RenderTargetType type) = 0;

		void AddDescriptorTableConfig(DescriptorTableConfig config) { m_DescriptorTableConfigs.push_back(config); }
		UINT GetInputResourceCount() const { return m_InputResourceCount; }

	protected:
		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList& m_CommandList;

		std::vector<GPUResource*> m_InputResources;
		std::vector<DescriptorTableConfig> m_DescriptorTableConfigs;
		std::vector<std::unique_ptr<RenderTexture>> m_RenderTargets;
		std::vector<RenderObject*> m_RenderObjects;

		UINT m_InputResourceCount = 0;
	};
}
