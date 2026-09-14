#pragma once
#include "RenderPass.h"
#include "../../Resources/Texture.h"
#include "../DrawItem.h"
#include "../Lighting/ClusteredLightGrid.h"

namespace DX12Engine
{
	class LightBuffer;

	class TransparentRenderPass : public RenderPass
	{
	public:
		TransparentRenderPass(RenderContext& context);
		~TransparentRenderPass();

		void Init() override;
		void Execute() override;
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetDrawItems(const std::vector<DrawItem>& drawItems) { m_DrawItems = drawItems; }
		void SetLightGridAddresses(const ClusteredLightGrid::LightGridFrameAddresses& addresses) { m_LightGridAddresses = addresses; }

	private:
		void CreatePSO() override;

		ClusteredLightGrid::LightGridFrameAddresses m_LightGridAddresses;
		std::vector<DrawItem> m_DrawItems;
		std::unique_ptr<Texture> m_FallbackEnvMap;
	};
}
