#pragma once
#include <d3dx12.h>
#include "../RenderContext.h"
#include "../Queues/CommandQueueManager.h"
#include "../RootSignatureBuilder.h"
#include "RenderPassData.h"
#include "../RenderPipelineConfig.h"

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
	class ConstantBuffer;
	class Camera;

	class RenderPass
	{
	public:
		RenderPass(RenderContext& context)
			: m_RenderContext(context), m_QueueManager(context.GetQueueManager()), m_CommandList(*m_QueueManager.GetGraphicsQueue().GetCommandList()), m_Camera(nullptr)
			{}
		~RenderPass() = default;
		virtual void Init();
		virtual void Execute();
		virtual void OnResize(DirectX::XMINT2 newSize);
		virtual RenderTexture* GetRenderTarget(RenderTargetType type) = 0;
		RenderPassType GetType() const { return m_Type; }

		void RebuildTransientDescriptors();

		void AddInputResources(std::vector<GPUResource*> resources)
		{
			m_InputResources.insert(m_InputResources.end(), resources.begin(), resources.end());
		}
		void AddInputResources(std::vector<std::shared_ptr<GPUResource>> resources)
		{
			m_InputResources.insert(m_InputResources.end(), resources.begin(), resources.end());
		}
		void AddResourceBlock(UINT size) { m_ResourceBlockSizes.push_back(size); }
		void SetVertexShader(const std::string& name) { m_VertexShaderName = name; }
		void SetPixelShader(const std::string& name) { m_PixelShaderName = name; }
		void SetRenderObjects(std::vector<RenderComponent*> renderObjects) { m_RenderObjects = renderObjects; }

		void SetCamera(Camera* camera) { m_Camera = camera; }

		void AddDescriptorTableConfig(DescriptorTableConfig config) { m_DescriptorTableConfigs.push_back(config); }
		void AddConstantBuffer(ConstantBuffer* cb) { m_ExternalCBs.push_back(cb); }

	protected:
		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList& m_CommandList;

		ScreenData m_ScreenData;
		std::unique_ptr<ConstantBuffer> m_ScreenDataCB;
		void UpdateCB();

		RenderPassType m_Type;
		std::vector<std::shared_ptr<GPUResource>> m_InputResources;
		std::vector<DescriptorTableConfig> m_DescriptorTableConfigs;
		std::vector<DescriptorHeapHandle> m_InputResourceBlockHandles;
		std::vector<std::unique_ptr<RenderTexture>> m_RenderTargets;
		std::vector<RenderComponent*> m_RenderObjects;
		std::string m_VertexShaderName;
		std::string m_PixelShaderName;
		std::vector<UINT> m_ResourceBlockSizes;

		Camera* m_Camera;
		std::vector<ConstantBuffer*> m_ExternalCBs;
	};
}
