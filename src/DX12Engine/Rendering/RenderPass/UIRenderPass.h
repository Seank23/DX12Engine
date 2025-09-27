#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	class ConstantBuffer;

	class UIRenderPass : public RenderPass
	{
	public:
		UIRenderPass(RenderContext& context);
		~UIRenderPass();

		void Init() override;
		void Execute() override;
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

	private:
		void CreateUIPassPSO();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

