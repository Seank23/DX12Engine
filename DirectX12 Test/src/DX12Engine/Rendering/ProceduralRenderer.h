#pragma once
#include "d3dx12.h"
#include "../Buffers/LightBuffer.h"
#include "../Heaps/DescriptorHeapHandle.h"
#include "../Resources/DepthMap.h"
#include "../Queues/CommandQueueManager.h"

namespace DX12Engine
{
	class RenderContext;
	class RenderObject;
	class DescriptorHeapManager;

	class ProceduralRenderer
	{
	public:
		ProceduralRenderer(RenderContext& context);
		~ProceduralRenderer();

		std::unique_ptr<DepthMap> CreateShadowMapResource();
		void RenderShadowMap(DepthMap* shadowMap, LightData lightSource, std::vector<RenderObject*> sceneObjects);

	private:
		void CreateShadowMapPSO(ID3D12RootSignature** outRootSignature, ID3D12PipelineState** outPipelineState);

		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		DescriptorHeapManager* m_HeapManager;
		ID3D12GraphicsCommandList* m_GraphicsCommandList;
	};
}

