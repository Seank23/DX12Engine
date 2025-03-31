#pragma once
#include "d3dx12.h"
#include "../Resources/Light.h"
#include "../Resources/DepthMap.h"
#include "../Queues/CommandQueueManager.h"
#include "../Buffers/ConstantBuffer.h"

namespace DX12Engine
{
	struct ShadowMapData
	{
		DirectX::XMMATRIX LightMVPMatrix;
		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMFLOAT3 LightPos;
		float FarPlane = 1.0f;
	};

	class RenderContext;
	class RenderObject;
	class DescriptorHeapManager;

	class ProceduralRenderer
	{
	public:
		ProceduralRenderer(RenderContext& context);
		~ProceduralRenderer();

		std::unique_ptr<DepthMap> CreateShadowMapResource(int numLights = 1);
		std::unique_ptr<DepthMap> CreateShadowCubeMapResource(int numLights = 1);
		void RenderShadowMaps(DepthMap* shadowMap, std::vector<Light*> lights, std::vector<RenderObject*> sceneObjects);
		void RenderShadowCubeMaps(DepthMap* shadowMap, std::vector<Light*> lights, std::vector<RenderObject*> sceneObjects);

	private:
		void CreateShadowMapPSO(ID3D12RootSignature** outRootSignature, ID3D12PipelineState** outPipelineState, bool isCubeMap);

		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		DescriptorHeapManager* m_HeapManager;
		ID3D12GraphicsCommandList* m_GraphicsCommandList;
	};
}

