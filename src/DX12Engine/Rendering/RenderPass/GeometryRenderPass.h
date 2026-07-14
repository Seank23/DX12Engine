#pragma once
#include "RenderPass.h"
#include "../DrawItem.h"

namespace DX12Engine
{
	class GeometryRenderPass : public RenderPass
	{
	public:
		GeometryRenderPass(RenderContext& context);
		~GeometryRenderPass();

		void Init() override;
		void Execute() override;

		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;
		void SetDrawItems(const std::vector<DrawItem>& drawItems) { m_DrawItems = drawItems; }

	private:
		void CreatePSO() override;

		std::vector<DrawItem> m_DrawItems;
	};
}
