#pragma once
#include "RenderPass.h"
#include "../DrawItem.h"
#include <DirectXMath.h>

namespace DX12Engine
{
	struct ShadowMapData
	{
		DirectX::XMMATRIX LightMVPMatrix;
		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMFLOAT3 LightPos;
		float FarPlane = 1.0f;
	};

	class Light;
	class RenderTexture;

	class ShadowMapRenderPass : public RenderPass
	{
	public:
		ShadowMapRenderPass(RenderContext& context, int shadowMapCount, bool isCubeMap);
		~ShadowMapRenderPass();

		void Init() override;
		void Execute() override;

		std::shared_ptr<RenderTexture> GetRenderTarget(ResourceSlot type) override;

		void SetLights(std::vector<Light*> lights) { m_Lights = lights; }
		void SetDrawItems(const std::vector<DrawItem>& drawItems) { m_DrawItems = drawItems; }

		std::shared_ptr<RenderTexture> GetShadowMapOutput() { return m_RenderTargets[0]; }

	private:
		void CreatePSO() override;
		void RenderShadowMap(RenderTexture* shadowMap, int lightIndex);
		void RenderShadowCubeMap(RenderTexture* shadowMap, int lightIndex);

		int m_ShadowMapCount;
		bool m_IsCubeMap;
		std::vector<Light*> m_Lights;
		std::vector<DrawItem> m_DrawItems;
		ShadowMapData m_ShadowMapData;
	};
}
