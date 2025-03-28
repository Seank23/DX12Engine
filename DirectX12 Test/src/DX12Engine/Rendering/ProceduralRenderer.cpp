#include "ProceduralRenderer.h"
#include "../Utils/Constants.h"
#include "./RenderContext.h"
#include "../Resources/ResourceManager.h"
#include "./RenderObject.h"
#include "../Heaps/DescriptorHeapManager.h"

namespace DX12Engine
{
	ProceduralRenderer::ProceduralRenderer(RenderContext& context)
		: m_RenderContext(context), m_QueueManager(context.GetQueueManager())
	{
		m_HeapManager = &(context.GetHeapManager());
		m_GraphicsCommandList = context.GetQueueManager().GetGraphicsQueue().GetCommandList();
	}

	ProceduralRenderer::~ProceduralRenderer()
	{
	}

	std::unique_ptr<DepthMap> ProceduralRenderer::CreateShadowMapResource()
	{
		auto device = m_RenderContext.GetDevice();
		D3D12_RESOURCE_DESC shadowMapDesc = {};
		shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		shadowMapDesc.Width = SHADOW_MAP_SIZE;
		shadowMapDesc.Height = SHADOW_MAP_SIZE;
		shadowMapDesc.DepthOrArraySize = 1;
		shadowMapDesc.MipLevels = 1;
		shadowMapDesc.Format = DXGI_FORMAT_D32_FLOAT;
		shadowMapDesc.SampleDesc.Count = 1;
		shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
		depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
		depthOptimizedClearValue.DepthStencil.Stencil = 0;

		ID3D12Resource* shadowMapResource = nullptr;
		auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&shadowMapDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&depthOptimizedClearValue,
			IID_PPV_ARGS(&shadowMapResource));

		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

		DescriptorHeapHandle dsvHandle = m_HeapManager->GetNewDSVDescriptorHeapHandle();
		device->CreateDepthStencilView(shadowMapResource, &dsvDesc, dsvHandle.GetCPUHandle());

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetRenderHeapHandleBlock(1);
		device->CreateShaderResourceView(shadowMapResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<DepthMap>(shadowMapResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, srvHandle, dsvHandle);
	}
	void ProceduralRenderer::RenderShadowMap(DepthMap* shadowMap, Light lightSource, std::vector<RenderObject*> sceneObjects)
	{
		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pipelineState = nullptr;
		CreateShadowMapPSO(&rootSignature, &pipelineState);

		if (!m_RenderContext.GetUploader().UploadAllPending()) // Upload any pending resources
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		m_GraphicsCommandList->SetPipelineState(pipelineState);
		m_GraphicsCommandList->SetGraphicsRootSignature(rootSignature);

		D3D12_VIEWPORT shadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, -1.0f, 1.0f };
		D3D12_RECT shadowScissorRect = { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
		m_GraphicsCommandList->RSSetViewports(1, &shadowViewport);
		m_GraphicsCommandList->RSSetScissorRects(1, &shadowScissorRect);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->GetResource(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		m_GraphicsCommandList->ResourceBarrier(1, &barrier);
		shadowMap->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto dsvHandle = shadowMap->GetDepthStencilDescriptor().GetCPUHandle();
		m_GraphicsCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
		m_GraphicsCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (RenderObject* object : sceneObjects)
		{
			DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(object->GetModelMatrix(), lightSource.ViewProjMatrix);

			m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, sizeof(mvpMatrix) / 4, &mvpMatrix, 0);
			auto vertexBufferView = object->m_VertexBuffer->GetVertexBufferView();
			auto indexBufferView = object->m_IndexBuffer->GetIndexBufferView();
			m_GraphicsCommandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			m_GraphicsCommandList->IASetIndexBuffer(&indexBufferView);
			m_GraphicsCommandList->DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
		}
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->GetResource(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		m_GraphicsCommandList->ResourceBarrier(1, &barrier);
		shadowMap->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	void ProceduralRenderer::CreateShadowMapPSO(ID3D12RootSignature** outRootSignature, ID3D12PipelineState** outPipelineState)
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterizerDesc.DepthBias = 1000;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.SlopeScaledDepthBias = 1.5f;

		pipelineStateBuilder = pipelineStateBuilder.ConfigureFromDefault()
			.SetRasterizerState(rasterizerDesc)
			.SetRenderTargetFormats(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT)
			.SetVertexShader(ResourceManager::GetInstance().GetShader("ShadowMap_VS"));
		CD3DX12_ROOT_PARAMETER param;
		param.InitAsConstants(sizeof(DirectX::XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootSignatureBuilder.AddCustomParam(param);
		
		*outRootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build()).Get();
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(*outRootSignature);
		*outPipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build()).Get();
	}
}
