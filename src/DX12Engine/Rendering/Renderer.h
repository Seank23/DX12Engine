#pragma once
#include "RenderContext.h"
#include "../Resources/Texture.h"
#include "Heaps/RenderPassDescriptorHeap.h"
#include "Buffers/LightBuffer.h"
#include "../Input/Camera.h"
#include "../Resources/RenderTexture.h"

namespace DX12Engine
{
	class RenderPass;
	class GameObject;
	struct RenderPipelineConfig;
	enum class RenderPassType;
	enum class RenderTargetType;

	struct RenderPipeline
	{
		std::vector<RenderPass*> RenderPasses;
	};

	class Renderer
	{
	public:
		Renderer(std::shared_ptr<RenderContext> context);
		~Renderer();

		bool PollWindow();
		void UpdateObjectList(std::vector<std::shared_ptr<GameObject>> objects);
		void ExecutePipeline(RenderPipeline pipeline);

		std::unique_ptr<std::vector<RenderTargetType>> GetTargets(std::vector<RenderTargetType> targets);
		RenderPipeline CreateRenderPipeline(RenderPipelineConfig config);

		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }
		void SetCamera(Camera* camera) { m_Camera = camera; }

		D3D12_VIEWPORT GetDefaultViewport();
		D3D12_RECT GetDefaultScissorRect();

	private:
		void PresentFrame(RenderTexture* finalRenderTarget);
		std::unique_ptr<RenderPass> GetRenderPass(RenderPassType type, int count);

		std::shared_ptr<RenderContext> m_RenderContext;
		CommandQueueManager& m_QueueManager;
		ID3D12GraphicsCommandList* m_CommandList;
		RenderPassDescriptorHeap& m_RenderHeap;

		LightBuffer* m_LightBuffer;
		Camera* m_Camera;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	};
}

