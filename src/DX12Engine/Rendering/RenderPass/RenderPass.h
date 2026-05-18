#pragma once
#include <d3dx12.h>
#include "../RenderContext.h"
#include "../Queues/CommandQueueManager.h"
#include "../RootSignatureBuilder.h"
#include "RenderPassData.h"
#include "../RenderPipelineConfig.h"

namespace DX12Engine
{
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
		virtual void OnResize(DirectX::XMINT2 newRenderSize);
		virtual std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) = 0;

		RenderPassType GetType() const { return m_Type; }

		void RebuildTransientDescriptors();
		void RemapInputResources(const std::unordered_map<GPUResource*, std::shared_ptr<GPUResource>>& resourceMap);
		void AppendResizeRemap(std::unordered_map<GPUResource*, std::shared_ptr<GPUResource>>& resourceMap) const;

		void AddInputResources(std::vector<std::shared_ptr<GPUResource>> resources)
		{
			m_InputResources.insert(m_InputResources.end(), resources.begin(), resources.end());
		}
		void AddResourceBlock(InputResourceType type, UINT size)
		{
			if (m_ResourceBlocks.find(type) == m_ResourceBlocks.end())
				m_ResourceBlockOrder.push_back(type);
			m_ResourceBlocks[type] = size;
		}
		void SetVertexShader(const std::string& name) { m_VertexShaderName = name; }
		void SetPixelShader(const std::string& name) { m_PixelShaderName = name; }
		void SetRenderObjects(std::vector<RenderComponent*> renderObjects) { m_RenderObjects = renderObjects; }

		void SetCamera(Camera* camera) { m_Camera = camera; }

		void AddDescriptorTableConfig(DescriptorTableConfig config) { m_DescriptorTableConfigs.push_back(config); }
		void AddConstantBuffer(ConstantBuffer* cb) { m_ExternalCBs.push_back(cb); }

	protected:
		virtual void CreatePSO() = 0;

		RenderContext& m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList& m_CommandList;

		RenderPassType m_Type;
		std::vector<std::shared_ptr<GPUResource>> m_InputResources;
		std::vector<DescriptorTableConfig> m_DescriptorTableConfigs;
		std::unordered_map<InputResourceType, DescriptorHeapHandle> m_InputResourceBlockHandles;
		std::vector<InputResourceType> m_ResourceBlockOrder;
		std::vector<std::shared_ptr<RenderTexture>> m_RenderTargets;
		std::vector<RenderComponent*> m_RenderObjects;
		std::string m_VertexShaderName;
		std::string m_PixelShaderName;
		std::unordered_map<InputResourceType, UINT> m_ResourceBlocks;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		Camera* m_Camera;
		std::vector<ConstantBuffer*> m_ExternalCBs;
		std::unordered_map<GPUResource*, std::shared_ptr<GPUResource>> m_LastResizeRemap;

		uint64_t m_LocalShaderGeneration = 0;
	};
}
