#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	class ConstantBuffer;
	class UISystem;

	class UIRenderPass : public RenderPass
	{
	public:
		UIRenderPass(RenderContext& context);
		~UIRenderPass();

		void Init() override;
		void Execute() override;
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetUISystem(UISystem* uiSystem) { m_UISystem = uiSystem; }

	private:
		void CreatePSO() override;

		UISystem* m_UISystem;
	};
}
