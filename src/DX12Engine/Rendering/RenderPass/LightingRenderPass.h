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
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }

	private:
		void CreatePSO() override;

		LightBuffer* m_LightBuffer;
		std::unique_ptr<Texture> m_FallbackEnvMap;
		std::unique_ptr<Texture> m_FallbackIrradianceMap;
		std::unique_ptr<ConstantBuffer> m_FallbackCascadedShadowCB;
	};
}

