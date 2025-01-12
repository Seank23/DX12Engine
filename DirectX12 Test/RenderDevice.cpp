#include "RenderDevice.h"
#include <stdexcept>

#define _DEBUG 1

namespace DX12Engine
{
	RenderDevice::RenderDevice()
		: m_Device(nullptr), m_CommandQueue(nullptr), m_Fence(nullptr), m_FenceEvent(nullptr)
	{
	}

	RenderDevice::~RenderDevice()
	{
		m_Device.Reset();
		m_CommandQueue.Reset();
        m_Fence.Reset();
        m_FenceEvent = nullptr;
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

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        m_Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueue));

        HRESULT fenceResult = m_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_Fence));
        if (FAILED(fenceResult)) {
            return;
        }
        m_FenceValue = 1;

        m_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_FenceEvent == nullptr) {
            fenceResult = HRESULT_FROM_WIN32(GetLastError());
            return;
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
		//psoDesc.DepthStencilState.DepthEnable = FALSE;
		//psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.DepthStencilState = depthStencilDesc;
		psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; // Match depth-stencil buffer format

		HRESULT psResult = m_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState));
		if (FAILED(psResult)) {
			throw std::runtime_error("Failed to create pipeline state. HRESULT: " + std::to_string(psResult));
		}
	}

	void RenderDevice::ResetCommandAllocatorAndList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		m_CommandAllocator->Reset();
		commandList->Reset(m_CommandAllocator.Get(), m_PipelineState.Get());
	}

	void RenderDevice::ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		m_CommandQueue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(commandList.GetAddressOf()));
	}

	void RenderDevice::UpdateFence()
	{
		// Signal and increment the fence value to sync the frame
		const UINT64 currentFenceValue = m_FenceValue;
		m_CommandQueue->Signal(m_Fence.Get(), currentFenceValue);
		m_FenceValue++;

		// Wait until the previous frame is done
		if (m_Fence->GetCompletedValue() < currentFenceValue) {
			m_Fence->SetEventOnCompletion(currentFenceValue, m_FenceEvent);
			WaitForSingleObject(m_FenceEvent, INFINITE);
		}
	}
	void RenderDevice::SetConstantBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferRes, ConstantBuffer constantBufferData)
	{
		// Map and initialize the constant buffer
		UINT* mappedData;
		constantBufferRes->Map(0, nullptr, reinterpret_cast<void**>(&mappedData));
		memcpy(mappedData, &constantBufferData, sizeof(ConstantBuffer));
		constantBufferRes->Unmap(0, nullptr);
	}
}
