#pragma once
#include "RenderPass.h"

namespace DX12Engine
{
	struct LightingPassData
	{
		DirectX::XMFLOAT4 CameraPosition;
		DirectX::XMMATRIX InvViewMatrix;
		DirectX::XMMATRIX InvProjectionMatrix;
		DirectX::XMFLOAT2 ScreenSize;
	};

	class LightBuffer;
	class ConstantBuffer;
	class Camera;

	class LightingRenderPass : public RenderPass
	{
	public:
		LightingRenderPass(RenderContext& context);
		~LightingRenderPass();

		void Init() override;
		void Execute() override;
		RenderTexture* GetRenderTarget(RenderTargetType type) override;

		void SetLightBuffer(LightBuffer* lightBuffer) { m_LightBuffer = lightBuffer; }
		void SetCamera(Camera* camera) { m_Camera = camera; }

	private:
		void CreateLightingPassPSO();
		void UpdateLightingPassCB();

		LightBuffer* m_LightBuffer;
		LightingPassData m_LightingPassData;
		Camera* m_Camera;

		std::unique_ptr<ConstantBuffer> m_LightingPassCB;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;

		D3D12_VIEWPORT m_Viewport;
		D3D12_RECT m_ScissorRect;
	};
}

