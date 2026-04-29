#pragma once
#include "RenderContext.h"
#include "../Resources/Texture.h"
#include "Heaps/RenderPassDescriptorHeap.h"
#include "../Resources/RenderTexture.h"
#include "../Entity/Scene.h"
#include "RenderPipelineConfig.h"
#include "RenderPass/RenderPass.h"
#include "RendererOptions.h"
#include <cstdint>
#include <unordered_map>

namespace DX12Engine
{
	class GameObject;
	struct ResolvedPrimitiveBinding;
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
		void ExecutePipeline(RenderPipeline pipeline, float frameTime);

		std::unique_ptr<std::vector<ResourceSlot>> GetTargets(std::vector<ResourceSlot> targets);
		RenderPipeline CreateRenderPipeline(RenderPipelineConfig config);

		void SetCurrentScene(Scene* scene) { m_CurrentScene = scene; }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

		void SetOptions(RendererOptions options);
		RendererOptions& GetOptions() { return m_Options; }

	private:
		void SetSceneData(RenderPipeline pipeline, float frameTime);
		void PresentFrame(RenderTexture* finalRenderTarget);
		std::unique_ptr<RenderPass> CreateRenderPass(RenderPassType type, int count);
		DirectX::XMMATRIX UpdateFrameJitter(DirectX::XMMATRIX projectionMatrix, DirectX::XMINT2 screenSize);
		static float Halton(uint32_t index, uint32_t base);
		void UpdatePostProcessingCB();

		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList* m_CommandList;
		RenderPassDescriptorHeap& m_RenderHeap;

		Scene* m_CurrentScene;
		UINT m_FrameIndex = 0;
		uint64_t m_JitterFrameIndex = 0;
		DirectX::XMFLOAT2 m_Jitter = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 m_PrevJitter = { 0.0f, 0.0f };

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		RendererOptions m_Options;
		std::unique_ptr<ConstantBuffer> m_PostProcessingCB;

		DirectX::XMMATRIX m_JitteredProjection;
		bool m_RequestTAAHistoryReset = true;

		uint64_t m_LocalShaderGeneration = 0;
		std::unordered_map<const ResolvedPrimitiveBinding*, UINT> m_BindingActiveLods;
	};
}

