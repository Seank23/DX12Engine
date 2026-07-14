#pragma once
#include "RenderPass.h"
#include "../DrawItem.h"
#include <DirectXMath.h>
#include "../RendererOptions.h"

namespace DX12Engine
{
	class Light;
	class RenderTexture;

	class CascadedShadowMapRenderPass : public RenderPass
	{
	public:
		CascadedShadowMapRenderPass(RenderContext& context);
		~CascadedShadowMapRenderPass();

		void Init() override;
		void Execute() override;

		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetDirectionalLight(const std::vector<Light*>& lights) { m_DirectionalLight = lights.empty() ? nullptr : lights[0]; }
		void SetDrawItems(const std::vector<DrawItem>& drawItems) { m_DrawItems = drawItems; }
		void SetSettings(const CSMSettings& settings) { m_Settings = settings; }
		ConstantBuffer* GetCascadedShadowCB() { return m_CascadedShadowCB.get(); }

		std::shared_ptr<RenderTexture> GetShadowMapOutput() { return m_RenderTargets[0]; }

	private:
		virtual void CreatePSO() override;
		void GenerateCascadeMatrices(int cascadeCount);

		Light* m_DirectionalLight = nullptr;
		std::vector<DrawItem> m_DrawItems;
		CascadedShadowData m_CascadedShadowData;
		std::unique_ptr<ConstantBuffer> m_CascadedShadowCB;
		CSMSettings m_Settings;

		std::vector<DirectX::XMMATRIX> m_CascadeMatrices;
	};
}
