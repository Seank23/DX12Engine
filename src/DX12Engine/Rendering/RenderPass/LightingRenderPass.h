#pragma once
#include "RenderPass.h"
#include "../../Resources/Texture.h"

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
		void SetHasSkybox(bool hasSkybox) { m_HasSkybox = hasSkybox; }

	private:
		void CreateLightingPassPSO();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		LightBuffer* m_LightBuffer;

		bool m_HasSkybox = false;
		std::unique_ptr<Texture> m_FallbackEnvMap;
		std::unique_ptr<Texture> m_FallbackIrradianceMap;
	};
}

