#include "ShadowMapRenderPass.h"
#include "../RenderContext.h"
#include "../../Resources/RenderTexture.h"
#include "../../Resources/ResourceManager.h"
#include "../../Utils/Constants.h"
#include "../RenderObject.h"
#include "../../Resources/Light.h"
#include "../../Utils/EngineUtils.h"

namespace DX12Engine
{
	ShadowMapRenderPass::ShadowMapRenderPass(RenderContext& context, int shadowMapCount, bool isCubeMap)
		: RenderPass(context), m_ShadowMapCount(shadowMapCount), m_IsCubeMap(isCubeMap)
	{
	}

	ShadowMapRenderPass::~ShadowMapRenderPass()
	{
	}

	void ShadowMapRenderPass::Init()
	{
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateDepthMap(
			DirectX::XMINT3(SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, m_ShadowMapCount),
			DXGI_FORMAT_D32_FLOAT,
			DXGI_FORMAT_R32_FLOAT,
			m_IsCubeMap));

		CreateShadowMapPSO();
	}

	void ShadowMapRenderPass::Execute()
	{
		RenderTexture* shadowMap = m_RenderTargets[0].get();
		for (int i = 0; i < m_Lights.size(); i++)
		{
			if (m_IsCubeMap)
				RenderShadowCubeMap(shadowMap, i);
			else
				RenderShadowMap(shadowMap, i);
		}
	}

	RenderTexture* ShadowMapRenderPass::GetRenderTarget(RenderTargetType type)
	{
		switch (type)
		{
		case RenderTargetType::Depth:
			return m_RenderTargets[0].get();
		default:
			return nullptr;
		}
	}

	void ShadowMapRenderPass::RenderShadowMap(RenderTexture* shadowMap, int lightIndex)
	{
		EngineUtils::Assert(lightIndex < shadowMap->GetTextureDescriptorCount());

		if (!m_RenderContext.GetUploader().UploadAllPending()) // Upload any pending resources
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

		D3D12_VIEWPORT shadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, -1.0f, 1.0f };
		D3D12_RECT shadowScissorRect = { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
		m_CommandList.RSSetViewports(1, &shadowViewport);
		m_CommandList.RSSetScissorRects(1, &shadowScissorRect);

		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->GetResource(),
			shadowMap->GetUsageState(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
		m_CommandList.ResourceBarrier(1, &barrier);
		shadowMap->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

		auto dsvHandle = shadowMap->GetTextureDescriptor(lightIndex).GetCPUHandle();
		m_CommandList.OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
		m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (RenderObject* object : m_RenderObjects)
		{
			DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(object->GetModelMatrix(), m_Lights[lightIndex]->GetViewProjMatrix());
			m_ShadowMapData.LightMVPMatrix = mvpMatrix;

			m_CommandList.SetGraphicsRoot32BitConstants(0, sizeof(ShadowMapData) / 4, &m_ShadowMapData, 0);
			auto vertexBufferView = object->GetVertexBufferView();
			auto indexBufferView = object->GetIndexBufferView();
			m_CommandList.IASetVertexBuffers(0, 1, &vertexBufferView);
			m_CommandList.IASetIndexBuffer(&indexBufferView);
			m_CommandList.DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
		}
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			shadowMap->GetResource(),
			shadowMap->GetUsageState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
		);
		m_CommandList.ResourceBarrier(1, &barrier);
		shadowMap->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	void ShadowMapRenderPass::RenderShadowCubeMap(RenderTexture* shadowMap, int lightIndex)
	{
		EngineUtils::Assert(lightIndex < shadowMap->GetTextureDescriptorCount());

		DirectX::XMVECTOR lightPos = DirectX::XMLoadFloat3(&m_Lights[lightIndex]->GetLightData().Position);
		DirectX::XMMATRIX lightProj = m_Lights[lightIndex]->GetLightData().ViewProjMatrix;
		DirectX::XMMATRIX shadowTransforms[6] = {
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // +X
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(-1.0f,0.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // -X
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f)), // +Y
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f,-1.0f, 0.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), // -Y
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)), // +Z
			DirectX::XMMatrixLookAtLH(lightPos, DirectX::XMVectorAdd(lightPos, DirectX::XMVectorSet(0.0f, 0.0f,-1.0f, 0.0f)), DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))  // -Z
		};
		m_ShadowMapData.FarPlane = m_Lights[lightIndex]->GetFarPlane();
		for (int j = 0; j < 6; j++)
		{
			DirectX::XMMATRIX lightViewProj = DirectX::XMMatrixMultiply(shadowTransforms[j], lightProj);

			if (!m_RenderContext.GetUploader().UploadAllPending()) // Upload any pending resources
				m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

			m_CommandList.SetPipelineState(m_PipelineState.Get());
			m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

			D3D12_VIEWPORT shadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, -1.0f, 1.0f };
			D3D12_RECT shadowScissorRect = { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
			m_CommandList.RSSetViewports(1, &shadowViewport);
			m_CommandList.RSSetScissorRects(1, &shadowScissorRect);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				shadowMap->GetResource(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE
			);
			m_CommandList.ResourceBarrier(1, &barrier);
			shadowMap->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

			auto dsvHandle = shadowMap->GetTextureDescriptor(j + 6 * lightIndex).GetCPUHandle();
			m_CommandList.OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
			m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			for (RenderObject* object : m_RenderObjects)
			{
				DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(object->GetModelMatrix(), lightViewProj);
				m_ShadowMapData.LightMVPMatrix = mvpMatrix;
				m_ShadowMapData.ModelMatrix = object->GetModelMatrix();
				m_ShadowMapData.LightPos = m_Lights[lightIndex]->GetLightData().Position;

				m_CommandList.SetGraphicsRoot32BitConstants(0, sizeof(ShadowMapData) / 4, &m_ShadowMapData, 0);
				auto vertexBufferView = object->GetVertexBufferView();
				auto indexBufferView = object->GetIndexBufferView();
				m_CommandList.IASetVertexBuffers(0, 1, &vertexBufferView);
				m_CommandList.IASetIndexBuffer(&indexBufferView);
				m_CommandList.DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
			}
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				shadowMap->GetResource(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
			);
			m_CommandList.ResourceBarrier(1, &barrier);
			shadowMap->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
			m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
		}
	}

	void ShadowMapRenderPass::CreateShadowMapPSO()
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
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT);

		if (m_IsCubeMap)
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

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
