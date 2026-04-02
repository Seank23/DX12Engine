#pragma once
#include "RenderPass.h"
#include "../../Resources/Texture.h"
#include "../DrawItem.h"

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
		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }

		void SetDrawItems(const std::vector<DrawItem>& drawItems) { m_DrawItems = drawItems; }

	private:
		void CreateTransparentPassPSO();

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		LightBuffer* m_LightBuffer;

		std::vector<DrawItem> m_DrawItems;

		std::unique_ptr<Texture> m_FallbackEnvMap;
	};
}

