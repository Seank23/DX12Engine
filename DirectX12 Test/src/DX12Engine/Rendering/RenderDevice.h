#pragma once
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <windows.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include "d3dx12.h"

#include "../Resources/Shader.h"
#include "../Buffers/ConstantBuffer.h"

namespace DX12Engine
{
	class PipelineStateCache;
	class RootSignatureCache;

	class RenderDevice
	{
	public:
		RenderDevice(PipelineStateCache& pipelineStateCache, RootSignatureCache& rootSignatureCache);
		~RenderDevice();

		void Init(HWND hwnd);
		void CreatePipelineState(Shader* vertexShader, Shader* pixelShader);

		Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return m_Device; }
		Microsoft::WRL::ComPtr<ID3D12RootSignature> GetRootSignature() const { return m_RootSignature; }
		Microsoft::WRL::ComPtr<ID3D12PipelineState> GetPipelineState() const { return m_PipelineState; }

	private:
		Microsoft::WRL::ComPtr<ID3D12Device> m_Device;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

		PipelineStateCache& m_PipelineStateCache;
		RootSignatureCache& m_RootSignatureCache;
	};
}
