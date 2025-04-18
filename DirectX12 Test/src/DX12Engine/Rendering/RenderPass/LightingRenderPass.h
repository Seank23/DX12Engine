#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	class LightBuffer;

	class LightingRenderPass : public RenderPass
	{
	public:
		LightingRenderPass(RenderContext& context);
		~LightingRenderPass();

		void Init() override;
		void Execute() override;
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }

	private:
		void CreateLightingPassPSO();

		LightBuffer* m_LightBuffer;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

