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
		void CreateGeometryPassPSO();

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		std::vector<DrawItem> m_DrawItems;
	};
}

