#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	class GeometryRenderPass : public RenderPass
	{
	public:
		GeometryRenderPass(RenderContext& context);
		~GeometryRenderPass();

		void Init() override;
		void Execute() override;

		RenderTexture* GetRenderTarget(RenderTargetType type) override;

	private:
		void CreateGeometryPassPSO();

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	};
}

