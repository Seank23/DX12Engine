#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	class DebugRenderPass : public RenderPass
	{
	public:
		DebugRenderPass(RenderContext& context);
		~DebugRenderPass();

		void Init() override;
		void Execute() override;
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

	private:
		void CreateDebugPassPSO();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

