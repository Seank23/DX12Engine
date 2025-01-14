#include "RenderDevice.h"
#include <stdexcept>

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
		m_CommandAllocator.Reset();
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

	void RenderDevice::InitCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& outCommandList)
    {
        m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_CommandAllocator));
        m_Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&outCommandList));
		outCommandList->Close();
    }

	void RenderDevice::CreatePipelineState(Shader* vertexShader, Shader* pixelShader)
	{
		CD3DX12_ROOT_PARAMETER rootParameters[1];
		rootParameters[0].InitAsConstantBufferView(0); // Bind to register b0

		// Root signature description
		CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;
		D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
		m_Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature));

		// Describe the vertex input layout
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
		depthStencilDesc.DepthEnable = TRUE;                // Enable depth testing
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // Write to the depth buffer
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // Pass if new depth is less than current
		depthStencilDesc.StencilEnable = FALSE;             // Disable stencil testing

		// Describe and create the graphics pipeline state object (PSO)
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader->GetShader().Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader->GetShader().Get());
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.DepthStencilState = depthStencilDesc;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // Match depth-stencil buffer format
		
		m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
	}

	void RenderDevice::ResetCommandAllocatorAndList(ID3D12GraphicsCommandList* commandList)
	{
		m_CommandAllocator->Reset();
		commandList->Reset(m_CommandAllocator.Get(), m_PipelineState.Get());
	}

	void RenderDevice::CreateConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource>& outConstantBufferRes)
	{
		D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ConstantBuffer));
		m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&outConstantBufferRes)
		);
	}

	void RenderDevice::SetConstantBuffer(ID3D12Resource* constantBufferRes, ConstantBuffer constantBufferData)
	{
		// Map and initialize the constant buffer
		UINT* mappedData;
		constantBufferRes->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		memcpy(mappedData, &constantBufferData, sizeof(ConstantBuffer));
		constantBufferRes->Unmap(0, nullptr);
	}
}
