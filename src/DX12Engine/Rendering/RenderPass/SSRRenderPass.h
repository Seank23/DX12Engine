#pragma once
#include "RenderPass.h"

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
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

	private:
		void CreateSSRPassPSO();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

