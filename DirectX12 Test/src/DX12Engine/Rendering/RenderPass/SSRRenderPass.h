#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	struct SSRPassData
	{
		DirectX::XMFLOAT4 CameraPosition;
		DirectX::XMMATRIX ViewMatrix;
		DirectX::XMMATRIX ProjectionMatrix;
		DirectX::XMMATRIX InvViewMatrix;
		DirectX::XMMATRIX InvProjectionMatrix;
		DirectX::XMFLOAT2 ScreenSize;
	};

	class RenderContext;
	class ConstantBuffer;
	class Camera;

	class SSRRenderPass : public RenderPass
	{
	public:
		SSRRenderPass(RenderContext& context);
		~SSRRenderPass();

		void Init() override;
		void Execute() override;
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

		void SetCamera(Camera* camera) { m_Camera = camera; }

	private:
		void CreateSSRPassPSO();
		void UpdateSSRPassCB();

		Camera* m_Camera;

		SSRPassData m_SSRPassData;
		std::unique_ptr<ConstantBuffer> m_SSRPassCB;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

