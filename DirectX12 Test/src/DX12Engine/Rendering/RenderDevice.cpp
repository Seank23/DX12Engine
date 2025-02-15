#include "RenderDevice.h"
#include <stdexcept>
#include "./PipelineStateCache.h"
#include "./PipelineStateBuilder.h"
#include "./RootSignatureCache.h"
#include "./RootSignatureBuilder.h"

#define _DEBUG 1

namespace DX12Engine
{
	RenderDevice::RenderDevice(PipelineStateCache& pipelineStateCache, RootSignatureCache& rootSignatureCache)
		: m_Device(nullptr), m_PipelineStateCache(pipelineStateCache), m_RootSignatureCache(rootSignatureCache)
	{
	}

	RenderDevice::~RenderDevice()
	{
		m_Device.Reset();
		m_PipelineState.Reset();
		m_RootSignature.Reset();
	}

	void RenderDevice::Init(HWND hwnd)
	{
        #if defined(_DEBUG)
            Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
                debugController->EnableDebugLayer();
            }
        #endif

        HRESULT deviceResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device));
        if (FAILED(deviceResult)) {
            MessageBox(hwnd, L"Failed to create DirectX 12 device.", L"Error", MB_OK);
            exit(-1);
        }
	}

	void RenderDevice::CreatePipelineState(Shader* vertexShader, Shader* pixelShader)
	{
		RootSignatureBuilder rootSigBuilder;
		D3D12_ROOT_SIGNATURE_DESC rootDesc = rootSigBuilder
			.AddConstantBuffer(0)
			.AddDescriptorTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0)
			.AddSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR)
			.Build();
		m_RootSignature = m_RootSignatureCache.GetOrCreateRootSignature(m_Device.Get(), rootDesc);

		PipelineStateBuilder psoBuilder;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = psoBuilder
			.AddInputLayout()
			.SetRootSignature(m_RootSignature.Get())
			.SetVertexShader(vertexShader->GetShader()->GetBufferPointer(), vertexShader->GetShader()->GetBufferSize())
			.SetPixelShader(pixelShader->GetShader()->GetBufferPointer(), pixelShader->GetShader()->GetBufferSize())
			.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargetFormats(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D24_UNORM_S8_UINT)
			.SetSampleDesc(UINT_MAX, 1, 0)
			.Build();
		m_PipelineState = m_PipelineStateCache.GetOrCreatePSO(m_Device.Get(), psoDesc);
	}
}
