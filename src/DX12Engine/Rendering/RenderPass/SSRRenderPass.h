#pragma once
#include "RenderPass.h"
#include "RenderPassData.h"

namespace DX12Engine
{
	class RenderContext;

	class SSRRenderPass : public RenderPass
	{
	public:
		SSRRenderPass(RenderContext& context);
		~SSRRenderPass();

		void Init() override;
		void Execute() override;
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

	private:
		void CreateSSRPassPSO();
		void TransitionHistoryBuffer(D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		// Ping-pong history buffers for temporal accumulation (index 0 = write, 1 = read)
		std::unique_ptr<RenderTexture> m_HistoryBuffers[2];
		int m_WriteIndex = 0;

		SSRTemporalData m_TemporalData;
		std::unique_ptr<ConstantBuffer> m_TemporalCB;
		uint32_t m_FrameIndex = 0;
		ScreenData m_PrevFrameScreenData;

		std::unique_ptr<Texture> m_FallbackEnvMap;
	};
}

