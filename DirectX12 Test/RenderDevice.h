#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include "Shader.h"
#include "ConstantBuffer.h"

namespace DX12Engine
{
	class RenderDevice
	{
	public:
		RenderDevice();
		~RenderDevice();

		void Init(HWND hwnd);
		void InitCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& outCommandList);
		void CreatePipelineState(Shader* vertexShader, Shader* pixelShader);

		void ResetCommandAllocatorAndList(ID3D12GraphicsCommandList* commandList);

		void CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& outConstantBufferRes);
		void SetConstantBuffer(ID3D12Resource* constantBufferRes, ConstantBuffer constantBufferData);

		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return m_Device; }
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_RootSignature; }
		Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const { return m_PipelineState; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;

		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CommandAllocator;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;
	};
}
