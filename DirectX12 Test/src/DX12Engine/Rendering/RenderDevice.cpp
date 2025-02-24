#include "RenderDevice.h"
#include <stdexcept>
#include "./PipelineStateBuilder.h"
#include "./RootSignatureBuilder.h"
#include "../Resources/ResourceManager.h"

#define _DEBUG 1

namespace DX12Engine
{
	RenderDevice::RenderDevice()
		: m_Device(nullptr)
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
		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSigBuilder.ConfigureFromDefault().Build());

		PipelineStateBuilder psoBuilder;
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = psoBuilder.ConfigureFromDefault()
			.SetRootSignature(m_RootSignature.Get())
			.SetVertexShader(vertexShader)
			.SetPixelShader(pixelShader)
			.Build();
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(psoDesc);
	}
}
