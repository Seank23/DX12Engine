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
		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

	private:
		void CreatePSO() override;
	};
}

