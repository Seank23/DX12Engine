#include "ProceduralRenderer.h"
#include "../Utils/Constants.h"
#include "./RenderContext.h"
#include "../Resources/ResourceManager.h"
#include "./RenderObject.h"
#include "../Heaps/DescriptorHeapHandle.h"

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

	std::unique_ptr<DepthMap> ProceduralRenderer::CreateShadowMapResource(int numLights)
	{
		auto device = m_RenderContext.GetDevice();
		D3D12_RESOURCE_DESC shadowMapDesc = {};
		shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		shadowMapDesc.Width = SHADOW_MAP_SIZE;
		shadowMapDesc.Height = SHADOW_MAP_SIZE;
		shadowMapDesc.DepthOrArraySize = numLights;
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

		std::vector<DescriptorHeapHandle> dsvDescriptors;
		for (int i = 0; i < numLights; i++)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;
			dsvDesc.Texture2DArray.MipSlice = 0;

			DescriptorHeapHandle dsvHandle = m_HeapManager->GetNewDSVDescriptorHeapHandle();
				device->CreateDepthStencilView(shadowMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
				dsvDescriptors.push_back(dsvHandle);
		}

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2DArray.ArraySize = numLights;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetRenderHeapHandleBlock(1);
		device->CreateShaderResourceView(shadowMapResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<DepthMap>(shadowMapResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, srvHandle, dsvDescriptors);
	}

	std::unique_ptr<DepthMap> ProceduralRenderer::CreateShadowCubeMapResource(int numLights)
	{
		auto device = m_RenderContext.GetDevice();
		D3D12_RESOURCE_DESC shadowMapDesc = {};
		shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		shadowMapDesc.Width = SHADOW_MAP_SIZE;
		shadowMapDesc.Height = SHADOW_MAP_SIZE;
		shadowMapDesc.DepthOrArraySize = numLights * 6;
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

		std::vector<DescriptorHeapHandle> dsvDescriptors;
		for (int i = 0; i < numLights * 6; i++)
		{
			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
			dsvDesc.Texture2DArray.FirstArraySlice = i;
			dsvDesc.Texture2DArray.ArraySize = 1;
			dsvDesc.Texture2DArray.MipSlice = 0;

			DescriptorHeapHandle dsvHandle = m_HeapManager->GetNewDSVDescriptorHeapHandle();
			device->CreateDepthStencilView(shadowMapResource, &dsvDesc, dsvHandle.GetCPUHandle());
			dsvDescriptors.push_back(dsvHandle);
		}
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.Texture2D.MipLevels = 1;
		srvDesc.Texture2DArray.ArraySize = numLights;

		DescriptorHeapHandle srvHandle = m_HeapManager->GetRenderHeapHandleBlock(1);
		device->CreateShaderResourceView(shadowMapResource, &srvDesc, srvHandle.GetCPUHandle());

		return std::make_unique<DepthMap>(shadowMapResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, srvHandle, dsvDescriptors, true);
	}

	void ProceduralRenderer::RenderShadowMaps(DepthMap* shadowMap, std::vector<Light*> lights, std::vector<RenderObject*> sceneObjects)
	{
		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pipelineState = nullptr;
		CreateShadowMapPSO(&rootSignature, &pipelineState, false);

		ShadowMapData shadowMapData;

		for (int i = 0; i < lights.size(); i++)
		{
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

			auto dsvHandle = shadowMap->GetDepthStencilDescriptor(i).GetCPUHandle();
			m_GraphicsCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
			m_GraphicsCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			for (RenderObject* object : sceneObjects)
			{
				DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(object->GetModelMatrix(), lights[i]->GetViewProjMatrix());
				shadowMapData.LightMVPMatrix = mvpMatrix;

				m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, sizeof(ShadowMapData) / 4, &shadowMapData, 0);
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
	}

	void ProceduralRenderer::RenderShadowCubeMaps(DepthMap* shadowMap, std::vector<Light*> lights, std::vector<RenderObject*> sceneObjects)
	{
		if (!shadowMap->GetIsCubeMap())
			throw std::runtime_error("Shadow map is not a cube map");

		ID3D12RootSignature* rootSignature = nullptr;
		ID3D12PipelineState* pipelineState = nullptr;
		CreateShadowMapPSO(&rootSignature, &pipelineState, true);

		ShadowMapData shadowMapData;

		for (int i = 0; i < lights.size(); i++)
		{
			DirectX::XMVECTOR lightPos = DirectX::XMLoadFloat3(&lights[i]->GetLightData().Position);
			DirectX::XMMATRIX lightProj = lights[i]->GetLightData().ViewProjMatrix;
			DirectX::XMMATRIX shadowTransforms[6] = {
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // +X
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(-1.0f,0.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // -X
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)), // +Y
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f,-1.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), // -Y
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // +Z
				DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 0.0f,-1.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))  // -Z
			};
			shadowMapData.FarPlane = lights[i]->GetFarPlane();
			for (int j = 0; j < 6; j++)
			{
				DirectX::XMMATRIX lightViewProj = DirectX::XMMatrixMultiply(shadowTransforms[j], lightProj);

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

				auto dsvHandle = shadowMap->GetDepthStencilDescriptor(j + 6 * i).GetCPUHandle();
				m_GraphicsCommandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
				m_GraphicsCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				m_GraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

				for (RenderObject* object : sceneObjects)
				{
					DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(object->GetModelMatrix(), lightViewProj);
					shadowMapData.LightMVPMatrix = mvpMatrix;
					shadowMapData.ModelMatrix = object->GetModelMatrix();
					shadowMapData.LightPos = lights[i]->GetLightData().Position;

					m_GraphicsCommandList->SetGraphicsRoot32BitConstants(0, sizeof(ShadowMapData) / 4, &shadowMapData, 0);
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
		}
	}

	void ProceduralRenderer::CreateShadowMapPSO(ID3D12RootSignature** outRootSignature, ID3D12PipelineState** outPipelineState, bool isCubeMap)
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		D3D12_RASTERIZER_DESC rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		rasterizerDesc.DepthBias = 1000;
		rasterizerDesc.DepthBiasClamp = 0.0f;
		rasterizerDesc.SlopeScaledDepthBias = 1.5f;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

		pipelineStateBuilder = pipelineStateBuilder.ConfigureFromDefault()
			.SetRasterizerState(rasterizerDesc)
			.SetRenderTargetFormats(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_D32_FLOAT);

		if (isCubeMap) 
		{
			pipelineStateBuilder = pipelineStateBuilder
				.SetVertexShader(ResourceManager::GetInstance().GetShader("ShadowCubeMap_VS"))
				.SetPixelShader(ResourceManager::GetInstance().GetShader("ShadowCubeMap_PS"));
		}
		else
		{
			pipelineStateBuilder = pipelineStateBuilder.SetVertexShader(ResourceManager::GetInstance().GetShader("ShadowMap_VS"));
		}

		CD3DX12_ROOT_PARAMETER param;
		param.InitAsConstants(sizeof(ShadowMapData) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootSignatureBuilder.AddCustomParam(param);
		
		*outRootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build()).Get();
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(*outRootSignature);
		*outPipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build()).Get();
	}
}
