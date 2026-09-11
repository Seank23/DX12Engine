#pragma once
#include "RenderPass.h"
#include "../../Resources/Texture.h"
#include "../Lighting/ClusteredLightGrid.h"

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

		void SetLightGridAddresses(const ClusteredLightGrid::LightGridFrameAddresses& addresses) { m_LightGridAddresses = addresses; }

	private:
		void CreatePSO() override;

		ClusteredLightGrid::LightGridFrameAddresses m_LightGridAddresses;
		std::unique_ptr<Texture> m_FallbackEnvMap;
		std::unique_ptr<Texture> m_FallbackIrradianceMap;
		std::unique_ptr<ConstantBuffer> m_FallbackCascadedShadowCB;
	};
}
