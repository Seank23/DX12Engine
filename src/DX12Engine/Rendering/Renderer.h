#pragma once
#include "RenderContext.h"
#include "../Resources/Texture.h"
#include "Heaps/RenderPassDescriptorHeap.h"
#include "../Resources/RenderTexture.h"
#include "../Entity/Scene.h"
#include "RenderPipelineConfig.h"
#include "RenderPass/RenderPass.h"

namespace DX12Engine
{
	class GameObject;
	struct RenderPipelineConfig;
	enum class RenderPassType;
	enum class ResourceSlot;

	struct RenderPipeline
	{
		std::vector<RenderPass*> RenderPasses;
		RenderPass* GetRenderPass(RenderPassType type)
		{
			for (RenderPass* pass : RenderPasses)
			{
				if (pass->GetType() == type)
					return pass;
			}
			return nullptr;
		}
	};

	class Renderer
	{
	public:
		Renderer(std::shared_ptr<RenderContext> context);
		~Renderer();

		bool PollWindow();
		void ExecutePipeline(RenderPipeline pipeline);

		std::unique_ptr<std::vector<ResourceSlot>> GetTargets(std::vector<ResourceSlot> targets);
		RenderPipeline CreateRenderPipeline(RenderPipelineConfig config);

		void SetCurrentScene(Scene* scene) { m_CurrentScene = scene; }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		void SetSceneData(RenderPipeline pipeline);
		void PresentFrame(RenderTexture* finalRenderTarget);
		std::unique_ptr<RenderPass> CreateRenderPass(RenderPassType type, int count);

		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList* m_CommandList;
		RenderPassDescriptorHeap& m_RenderHeap;

		Scene* m_CurrentScene;
		UINT m_FrameIndex = 0;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	};
}

