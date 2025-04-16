#pragma once
#include "RenderPass.h"
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

		RenderTexture* GetRenderTarget(RenderTargetType type) override;

		void SetLights(std::vector<Light*> lights) { m_Lights = lights; }

		RenderTexture* GetShadowMapOutput() { return static_cast<RenderTexture*>(m_RenderTargets[0].get()); }

	private:
		void RenderShadowMap(RenderTexture* shadowMap, int lightIndex);
		void RenderShadowCubeMap(RenderTexture* shadowMap, int lightIndex);
		void CreateShadowMapPSO();

		int m_ShadowMapCount;
		bool m_IsCubeMap;
		std::vector<Light*> m_Lights;
		ShadowMapData m_ShadowMapData;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
	};
}

