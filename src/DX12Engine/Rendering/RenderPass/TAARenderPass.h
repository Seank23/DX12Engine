#pragma once
#include "RenderPass.h"
#include "RenderPassData.h"
#include "../RendererOptions.h"

namespace DX12Engine
{
	class RenderContext;

	class TAARenderPass : public RenderPass
	{
	public:
		TAARenderPass(RenderContext& context);
		~TAARenderPass();
		void Init() override;
		void Execute() override;
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetJitterStates(const DirectX::XMFLOAT2& jitter, const DirectX::XMFLOAT2& prevJitter) { m_Jitter = jitter; m_PrevJitter = prevJitter; }
		void SetSettings(const TAASettings& settings) { m_Settings = settings; }
		void InvalidateHistory() { m_ForceHistoryReset = true; }

	private:
		void CreateTAAPassPSO();
		void TransitionHistoryBuffer(D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
		static float MatrixMaxAbsDelta(const DirectX::XMMATRIX& a, const DirectX::XMMATRIX& b);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		// Ping-pong history buffers for temporal accumulation (index 0 = write, 1 = read)
		std::unique_ptr<RenderTexture> m_HistoryBuffers[2];
		int m_WriteIndex = 0;

		TAATemporalData m_TemporalData;
		std::unique_ptr<ConstantBuffer> m_TemporalCB;
		uint32_t m_FrameIndex = 0;
		ScreenData m_PrevFrameScreenData;

		DirectX::XMFLOAT2 m_Jitter = { 0.0f, 0.0f };
		DirectX::XMFLOAT2 m_PrevJitter = { 0.0f, 0.0f };
		TAASettings m_Settings;
		bool m_ForceHistoryReset = true;
		bool m_HistoryValid = false;
		std::unique_ptr<RenderTexture> m_FallbackReactiveMask;
		bool m_UsingFallbackReactiveMask = false;
	};
}

