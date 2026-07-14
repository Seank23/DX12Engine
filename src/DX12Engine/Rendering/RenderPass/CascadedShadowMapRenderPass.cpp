#define NOMINMAX
#include "CascadedShadowMapRenderPass.h"
#include "../RenderContext.h"
#include "../../Resources/RenderTexture.h"
#include "../../Resources/ResourceManager.h"
#include "../../Utils/Constants.h"
#include "../../Entity/RenderComponent.h"
#include "../../Asset/MeshPrimitive.h"
#include "../../Resources/Light.h"
#include "../../Utils/EngineUtils.h"
#include "../../Input/Camera.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace DX12Engine
{
	struct ShadowMapData
	{
		DirectX::XMMATRIX LightMVPMatrix;
	};

	CascadedShadowMapRenderPass::CascadedShadowMapRenderPass(RenderContext& context)
		: RenderPass(context)
	{
		m_Type = RenderPassType::CascadedShadowMap;
	}

	CascadedShadowMapRenderPass::~CascadedShadowMapRenderPass()
	{
	}

	void CascadedShadowMapRenderPass::Init()
	{
		RenderPass::Init();
		m_Settings.CascadeCount = (std::clamp)(m_Settings.CascadeCount, 1, MAX_CSM_CASCADES);

		DirectX::XMINT3 renderSize{ m_Settings.ShadowMapSize, m_Settings.ShadowMapSize, m_Settings.CascadeCount };
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateDepthMap(
			RenderTextureConfig{ renderSize, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D32_FLOAT, 1, { 0.0f, 0.0f, 0.0f, 1.0f }, false, false }));

		m_CascadedShadowCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(CascadedShadowData));

		CreatePSO();
	}

	void CascadedShadowMapRenderPass::Execute()
	{
		RenderTexture* shadowMap = m_RenderTargets[0].get();

		if (!m_DirectionalLight || !m_Camera)
		{
			if (shadowMap->GetUsageState() != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
			{
				m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

				auto barrierToWrite = CD3DX12_RESOURCE_BARRIER::Transition(
					shadowMap->GetResource(),
					shadowMap->GetUsageState(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE);
				m_CommandList.ResourceBarrier(1, &barrierToWrite);
				shadowMap->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

				auto dsvHandle = shadowMap->GetTextureDescriptor(0).GetCPUHandle();
				m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

				auto barrierToRead = CD3DX12_RESOURCE_BARRIER::Transition(
					shadowMap->GetResource(),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				m_CommandList.ResourceBarrier(1, &barrierToRead);
				shadowMap->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
				m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
			}
			return;
		}

		const int activeCascadeCount = (std::min)((std::clamp)(m_Settings.CascadeCount, 1, MAX_CSM_CASCADES), shadowMap->GetTextureDescriptorCount());
		if (activeCascadeCount <= 0)
			return;

		GenerateCascadeMatrices(activeCascadeCount);
		ShadowMapData shadowMapData;

		for (int i = 0; i < activeCascadeCount; i++)
		{
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

			m_CommandList.SetPipelineState(m_PipelineState.Get());
			m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());
			D3D12_VIEWPORT shadowViewport = { 0.0f, 0.0f, (float)SHADOW_MAP_SIZE, (float)SHADOW_MAP_SIZE, 0.0f, 1.0f };
			D3D12_RECT shadowScissorRect = { 0, 0, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE };
			m_CommandList.RSSetViewports(1, &shadowViewport);
			m_CommandList.RSSetScissorRects(1, &shadowScissorRect);

			auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				shadowMap->GetResource(),
				shadowMap->GetUsageState(),
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			m_CommandList.ResourceBarrier(1, &barrier);
			shadowMap->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);

			auto dsvHandle = shadowMap->GetTextureDescriptor(i).GetCPUHandle();
			m_CommandList.OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
			m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			MeshPrimitive* lastPrimitive = nullptr;
			UINT lastLodLevel = UINT_MAX;
			for (const DrawItem& item : m_DrawItems)
			{
				if (!item.Primitive)
					continue;

				DirectX::XMMATRIX mvpMatrix = DirectX::XMMatrixMultiply(item.ModelMatrix, m_CascadeMatrices[i]);
				shadowMapData.LightMVPMatrix = mvpMatrix;
				m_CommandList.SetGraphicsRoot32BitConstants(0, sizeof(ShadowMapData) / 4, &shadowMapData, 0);

				if (item.Primitive != lastPrimitive)
				{
					item.Primitive->SetActiveLOD(item.ActiveLODLevel);
					auto vertexBufferView = item.Primitive->GetVertexBufferView();
					auto indexBufferView = item.Primitive->GetActiveIndexBufferView();
					m_CommandList.IASetVertexBuffers(0, 1, &vertexBufferView);
					m_CommandList.IASetIndexBuffer(&indexBufferView);
					lastPrimitive = item.Primitive;
					lastLodLevel = item.ActiveLODLevel;
				}
				else if (item.ActiveLODLevel != lastLodLevel)
				{
					item.Primitive->SetActiveLOD(item.ActiveLODLevel);
					auto indexBufferView = item.Primitive->GetActiveIndexBufferView();
					m_CommandList.IASetIndexBuffer(&indexBufferView);
					lastLodLevel = item.ActiveLODLevel;
				}

				m_CommandList.DrawIndexedInstanced(item.IndexCount, 1, item.FirstIndex, item.BaseVertex, 0);
			}
			barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				shadowMap->GetResource(),
				shadowMap->GetUsageState(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			m_CommandList.ResourceBarrier(1, &barrier);
			shadowMap->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

			m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
		}
	}

	std::shared_ptr<RenderTexture> CascadedShadowMapRenderPass::GetRenderTarget(ResourceSlot type)
	{
		switch (type)
		{
		case ResourceSlot::Depth:
			return m_RenderTargets[0];
		default:
			return nullptr;
		}
	}

	void CascadedShadowMapRenderPass::GenerateCascadeMatrices(int cascadeCount)
	{
		if (!m_Camera || !m_CascadedShadowCB)
			return;

		const float nearZ = m_Camera ? m_Camera->GetNearPlane() : 0.1f;
		const float farZ = (std::max)(nearZ + 0.01f, (std::min)(m_Camera->GetFarPlane(), m_Settings.MaxDistance));
		const float splitLambda = (std::clamp)(m_Settings.SplitLambda, 0.0f, 1.0f);

		std::vector<float> cascadeEnds(cascadeCount + 1);
		cascadeEnds[0] = nearZ;
		for (int i = 1; i <= cascadeCount; i++)
		{
			const float p = static_cast<float>(i) / static_cast<float>(cascadeCount);
			const float logSplit = nearZ * std::pow(farZ / nearZ, p);
			const float uniformSplit = nearZ + (farZ - nearZ) * p;
			cascadeEnds[i] = std::lerp(uniformSplit, logSplit, splitLambda);
		}

		const float cameraAspect = (std::max)(m_Camera->GetAspectRatio(), 0.0001f);
		const float cameraFovYRadians = DirectX::XMConvertToRadians(m_Camera->GetFOV());
		const float tanHalfFovY = std::tan(cameraFovYRadians * 0.5f);
		const float tanHalfFovX = tanHalfFovY * cameraAspect;
		const DirectX::XMMATRIX invView = DirectX::XMMatrixInverse(nullptr, m_Camera->GetViewMatrix());

		DirectX::XMFLOAT3 lightDirF = m_DirectionalLight->GetDirection();
		DirectX::XMVECTOR lightDir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&lightDirF));
		if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(lightDir)) < 1e-6f)
			lightDir = DirectX::XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float upDot = std::fabs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(lightDir, worldUp)));
		if (upDot > 0.99f)
			worldUp = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		m_CascadeMatrices.assign(cascadeCount, DirectX::XMMatrixIdentity());
		std::vector<float> texelSizes(MAX_CSM_CASCADES, 0.0f);
		std::array<float, MAX_CSM_CASCADES> splitEndsForShader{ farZ, farZ, farZ, farZ, farZ, farZ, farZ, farZ };

		for (int i = 0; i < cascadeCount; i++)
		{
			const float cascadeNear = cascadeEnds[i];
			const float cascadeFar = cascadeEnds[i + 1];
			splitEndsForShader[i] = cascadeFar;

			const float nearY = cascadeNear * tanHalfFovY;
			const float nearX = cascadeNear * tanHalfFovX;
			const float farY = cascadeFar * tanHalfFovY;
			const float farX = cascadeFar * tanHalfFovX;

			const DirectX::XMVECTOR cornersVS[8] = {
				DirectX::XMVectorSet(-nearX, nearY, cascadeNear, 1.0f),
				DirectX::XMVectorSet(nearX, nearY, cascadeNear, 1.0f),
				DirectX::XMVectorSet(nearX, -nearY, cascadeNear, 1.0f),
				DirectX::XMVectorSet(-nearX, -nearY, cascadeNear, 1.0f),
				DirectX::XMVectorSet(-farX, farY, cascadeFar, 1.0f),
				DirectX::XMVectorSet(farX, farY, cascadeFar, 1.0f),
				DirectX::XMVectorSet(farX, -farY, cascadeFar, 1.0f),
				DirectX::XMVectorSet(-farX, -farY, cascadeFar, 1.0f)
			};

			DirectX::XMVECTOR cornersWS[8];
			DirectX::XMVECTOR centerWS = DirectX::XMVectorZero();
			for (int c = 0; c < 8; c++)
			{
				cornersWS[c] = DirectX::XMVector3TransformCoord(cornersVS[c], invView);
				centerWS = DirectX::XMVectorAdd(centerWS, cornersWS[c]);
			}
			centerWS = DirectX::XMVectorScale(centerWS, 1.0f / 8.0f);

			float radius = 0.0f;
			for (int c = 0; c < 8; c++)
			{
				const DirectX::XMVECTOR dist = DirectX::XMVector3Length(DirectX::XMVectorSubtract(cornersWS[c], centerWS));
				radius = (std::max)(radius, DirectX::XMVectorGetX(dist));
			}
			radius = std::ceil(radius * 16.0f) / 16.0f;
			const float worldUnitsPerTexel = (2.0f * radius) / static_cast<float>(SHADOW_MAP_SIZE);
			texelSizes[i] = worldUnitsPerTexel;

			DirectX::XMVECTOR eye = DirectX::XMVectorSubtract(centerWS, DirectX::XMVectorScale(lightDir, radius * 2.5f));
			DirectX::XMMATRIX lightView = DirectX::XMMatrixLookAtLH(eye, centerWS, worldUp);

			DirectX::XMVECTOR centerLS = DirectX::XMVector3TransformCoord(centerWS, lightView);
			centerLS = DirectX::XMVectorSet(
				std::floor(DirectX::XMVectorGetX(centerLS) / worldUnitsPerTexel) * worldUnitsPerTexel,
				std::floor(DirectX::XMVectorGetY(centerLS) / worldUnitsPerTexel) * worldUnitsPerTexel,
				DirectX::XMVectorGetZ(centerLS),
				1.0f);
			const DirectX::XMMATRIX lightViewInv = DirectX::XMMatrixInverse(nullptr, lightView);
			centerWS = DirectX::XMVector3TransformCoord(centerLS, lightViewInv);
			eye = DirectX::XMVectorSubtract(centerWS, DirectX::XMVectorScale(lightDir, radius * 2.5f));
			lightView = DirectX::XMMatrixLookAtLH(eye, centerWS, worldUp);

			DirectX::XMVECTOR cornersLS[8];
			for (int c = 0; c < 8; c++)
				cornersLS[c] = DirectX::XMVector3TransformCoord(cornersWS[c], lightView);

			float minZ = FLT_MAX;
			float maxZ = -FLT_MAX;
			float minX = FLT_MAX;
			float maxX = -FLT_MAX;
			float minY = FLT_MAX;
			float maxY = -FLT_MAX;
			for (int c = 0; c < 8; c++)
			{
				const float z = DirectX::XMVectorGetZ(cornersLS[c]);
				minZ = (std::min)(minZ, z);
				maxZ = (std::max)(maxZ, z);
				const float x = DirectX::XMVectorGetX(cornersLS[c]);
				minX = (std::min)(minX, x);
				maxX = (std::max)(maxX, x);
				const float y = DirectX::XMVectorGetY(cornersLS[c]);
				minY = (std::min)(minY, y);
				maxY = (std::max)(maxY, y);
			}

			const float zPadding = (std::max)(10.0f, radius);
			const float nearPlane = (std::max)(0.1f, minZ - zPadding);
			const float farPlane = (std::max)(nearPlane + 1.0f, maxZ + zPadding);
			const DirectX::XMMATRIX lightProj = DirectX::XMMatrixOrthographicOffCenterLH(
				minX, maxX, minY, maxY, nearPlane, farPlane);

			m_CascadeMatrices[i] = DirectX::XMMatrixMultiply(lightView, lightProj);
		}

		for (int i = 0; i < MAX_CSM_CASCADES; i++)
		{
			m_CascadedShadowData.CascadeViewProj[i] = (i < cascadeCount) ? m_CascadeMatrices[i] : DirectX::XMMatrixIdentity();
			m_CascadedShadowData.CascadeSplits[i].Value = splitEndsForShader[i];
			m_CascadedShadowData.CascadeTexelSize[i].Value = texelSizes[i];
		}
		m_CascadedShadowData.Params0 = DirectX::XMFLOAT4(static_cast<float>(cascadeCount), farZ, m_Settings.CascadeBlend, 0.0f);
		m_CascadedShadowData.BiasParams = DirectX::XMFLOAT4(m_Settings.ConstantBias, m_Settings.SlopeBias, m_Settings.NormalBias, 0.0f);
		m_CascadedShadowCB->Update(&m_CascadedShadowData, sizeof(CascadedShadowData));
	}

	void CascadedShadowMapRenderPass::CreatePSO()
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
								   .SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT)
								   .SetVertexShader(ResourceManager::GetInstance().GetShader("ShadowMap_VS"));

		CD3DX12_ROOT_PARAMETER param;
		param.InitAsConstants(sizeof(ShadowMapData) / 4, 0, 0, D3D12_SHADER_VISIBILITY_ALL);
		rootSignatureBuilder.AddCustomParam(param);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
