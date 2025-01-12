#pragma once
#include "RenderDevice.h"
#include "RenderWindow.h"

namespace DX12Engine
{
	class RenderContext
	{
	public:
		RenderContext(int width, int height);
		~RenderContext();

		void CreatePipeline(Shader* vertexShader, Shader* pixelShader);

		DirectX::XMFLOAT2							GetWindowSize() const { return m_WindowSize; }
		HWND										GetWindowHandle() const { return m_RenderWindow->GetWindowHandle(); }
		Microsoft::WRL::ComPtr<ID3D12Device>		GetDevice() const { return m_RenderDevice->GetDevice(); }
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_RenderDevice->GetRootSignature(); }
		CD3DX12_CPU_DESCRIPTOR_HANDLE				GetRTVHandle() const { return m_RenderWindow->GetRTVHandle(); }
		D3D12_CPU_DESCRIPTOR_HANDLE					GetDSVHandle() const { return m_RenderWindow->GetDSVHandle(); }
		Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const { return m_RenderDevice->GetPipelineState(); }

		CD3DX12_RESOURCE_BARRIER	TransitionRenderTarget(bool forward) const { return m_RenderWindow->TransitionRenderTarget(forward); }
		bool						ProcessWindowMessages() const { return m_RenderWindow->ProcessWindowMessages(); }
		void						InitCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& outCommandList) const { m_RenderDevice->InitCommandList(outCommandList); }
		void						ResetCommandAllocatorAndList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) const { m_RenderDevice->ResetCommandAllocatorAndList(commandList); }
		void						ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList) const { m_RenderDevice->ExecuteCommandList(commandList); }
		void						PresentFrame() const { m_RenderWindow->PresentFrame(); }
		void						UpdateFence() const { m_RenderDevice->UpdateFence(); }
		void						CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& outConstantBufferRes) const { m_RenderDevice->CreateConstantBuffer(outConstantBufferRes); }
		void						SetConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferRes, ConstantBuffer constantBufferData) const { m_RenderDevice->SetConstantBuffer(constantBufferRes, constantBufferData); }

	private:
		std::unique_ptr<RenderWindow> m_RenderWindow;
		std::unique_ptr<RenderDevice> m_RenderDevice;

		DirectX::XMFLOAT2 m_WindowSize;
	};
}

