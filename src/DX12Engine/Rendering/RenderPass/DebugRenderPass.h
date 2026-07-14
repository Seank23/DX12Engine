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
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

	private:
		void CreatePSO() override;
	};
}
