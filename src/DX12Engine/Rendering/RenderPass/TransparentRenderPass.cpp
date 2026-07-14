#include "TransparentRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../PipelineStateBuilder.h"
#include "../RootSignatureBuilder.h"
#include "../Buffers/LightBuffer.h"
#include "../RenderUtils.h"
#include "../../Asset/MaterialTemplate.h"
#include "../../Asset/MeshPrimitive.h"

namespace DX12Engine
{
	TransparentRenderPass::TransparentRenderPass(RenderContext& context)
		: RenderPass(context)
	{
		m_Type = RenderPassType::Transparent;
	}

	TransparentRenderPass::~TransparentRenderPass()
	{
	}

	void TransparentRenderPass::Init()
	{
		// If no ExternalTextures block was registered (env cubemap), inject a neutral
		// gray fallback at block 0 so the env map slot is always bound.
		const bool hasEnvMap = m_ResourceBlocks.find(InputResourceType::EnvironmentMap) != m_ResourceBlocks.end();
		if (!hasEnvMap)
		{
			m_FallbackEnvMap = ResourceManager::GetInstance().CreateDefaultCubeMap();
			m_InputResources.insert(m_InputResources.begin(), std::shared_ptr<GPUResource>(m_FallbackEnvMap.get(), [](GPUResource*) {}));
			AddResourceBlock(InputResourceType::EnvironmentMap, 1);
		}

		RenderPass::Init();

		m_VertexShaderName = m_VertexShaderName.empty() ? "PBRTransparent_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "PBRTransparent_PS" : m_PixelShaderName;

		DirectX::XMINT3 renderSize{ m_RenderContext.GetRenderSize().x, m_RenderContext.GetRenderSize().y, 1 };
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(RenderTextureConfig{ renderSize, DXGI_FORMAT_R16G16B16A16_FLOAT }));

		m_Viewport = { 0.0f, 0.0f, (float)renderSize.x, (float)renderSize.y, 0.0f, 1.0f };
		m_ScissorRect = { 0, 0, (LONG)renderSize.x, (LONG)renderSize.y };

		CreatePSO();
	}

	void TransparentRenderPass::Execute()
	{
		RenderPass::Execute();

		RenderTexture* renderTarget = m_RenderTargets[0].get();
		RenderTexture* sceneSource = dynamic_cast<RenderTexture*>(m_InputResources[m_InputResources.size() - 2].get());

		RenderUtils::UpdateMaterialBindings(m_DrawItems);

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

		m_CommandList.RSSetViewports(1, &m_Viewport);
		m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		if (sceneSource && sceneSource->GetResource()->GetDesc().Format == renderTarget->GetResource()->GetDesc().Format)
		{
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				sceneSource->GetResource(),
				sceneSource->GetUsageState(),
				D3D12_RESOURCE_STATE_COPY_SOURCE));
			sceneSource->SetUsageState(D3D12_RESOURCE_STATE_COPY_SOURCE);
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				renderTarget->GetResource(),
				renderTarget->GetUsageState(),
				D3D12_RESOURCE_STATE_COPY_DEST));
			renderTarget->SetUsageState(D3D12_RESOURCE_STATE_COPY_DEST);
			m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
			m_CommandList.CopyResource(renderTarget->GetResource(), sceneSource->GetResource());

			barriers.clear();
			barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				sceneSource->GetResource(),
				sceneSource->GetUsageState(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
			sceneSource->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}

		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_RENDER_TARGET));
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
		m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderTarget->GetTextureDescriptor().GetCPUHandle();
		m_CommandList.OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
		m_CommandList.SetDescriptorHeaps(1, &srvHeap);
		m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto bindPassInputTables = [this]()
		{
			m_CommandList.SetGraphicsRootConstantBufferView(1, m_RenderContext.GetScreenDataBuffer().GetGPUAddress());
			m_CommandList.SetGraphicsRootDescriptorTable(4, m_InputResourceBlockHandles[InputResourceType::EnvironmentMap].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(5, m_InputResourceBlockHandles[InputResourceType::SceneColor].GetGPUHandle());
			m_CommandList.SetGraphicsRootDescriptorTable(6, m_InputResourceBlockHandles[InputResourceType::Depth].GetGPUHandle());
		};
		bindPassInputTables();

		D3D12_GPU_VIRTUAL_ADDRESS lastCBV = 0;
		Material* lastMaterial = nullptr;
		MeshPrimitive* lastPrimitive = nullptr;
		UINT lastLodLevel = UINT_MAX;
		uint64_t lastPipelineKey = UINT64_MAX;

		for (const DrawItem& item : m_DrawItems)
		{
			if (!item.Primitive || !item.Material)
				continue;

			if (item.BlendMode != AlphaMode::Blend)
				continue;

			MaterialTemplate* tmpl = item.Template;
			const bool hasTemplatePSO = tmpl && tmpl->GetPassTarget() == PassTarget::Transparent && tmpl->HasResolvedPSO();

			if (!hasTemplatePSO)
			{
				m_CommandList.SetPipelineState(m_PipelineState.Get());
				m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());
				bindPassInputTables();
				lastPipelineKey = UINT64_MAX;
				lastCBV = 0;
				lastMaterial = nullptr;
				lastPrimitive = nullptr;
			}
			else if (item.PipelineKey != lastPipelineKey)
			{
				m_CommandList.SetPipelineState(tmpl->GetPipelineState());
				m_CommandList.SetGraphicsRootSignature(tmpl->GetRootSignature());
				bindPassInputTables();
				lastPipelineKey = item.PipelineKey;
				lastCBV = 0;
				lastMaterial = nullptr;
				lastPrimitive = nullptr;
			}

			if (item.CBVAddress != lastCBV)
			{
				m_CommandList.SetGraphicsRootConstantBufferView(0, item.CBVAddress);
				lastCBV = item.CBVAddress;
			}

			if (item.Material != lastMaterial)
			{
				item.Material->Bind(&m_CommandList, 2, 3);
				lastMaterial = item.Material;
			}

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

		barriers.clear();
		barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			renderTarget->GetResource(),
			renderTarget->GetUsageState(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
		renderTarget->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		m_CommandList.ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

		m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
	}

	std::shared_ptr<RenderTexture> TransparentRenderPass::GetRenderTarget(ResourceSlot type)
	{
		switch (type)
		{
		case ResourceSlot::Composite:
			return m_RenderTargets[0];
		default:
			return nullptr;
		}
	}

	void TransparentRenderPass::CreatePSO()
	{
		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;
		D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		D3D12_DEPTH_STENCIL_DESC depthDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		depthDesc.DepthEnable = FALSE;
		depthDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

		// Transparent root signature layout:
		// b0 object, b1 material, b2 screen data,
		// table2=t0..t5 material textures, table3=t6 opaque scene,
		// table4=t7 env map, table5=t8 G-buffer depth.
		std::vector<DescriptorTableConfig> tables;
		tables.emplace_back(6, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		tables.emplace_back(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6);
		tables.emplace_back(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 7);
		tables.emplace_back(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 8);

		pipelineStateBuilder = pipelineStateBuilder
								   .ConfigureFromDefault(ResourceManager::GetInstance().GetShader(m_VertexShaderName), ResourceManager::GetInstance().GetShader(m_PixelShaderName))
								   .SetBlendState(blendDesc)
								   .SetDepthStencilState(depthDesc)
								   .SetRenderTargets({ DXGI_FORMAT_R16G16B16A16_FLOAT });

		rootSignatureBuilder = rootSignatureBuilder
								   .AddConstantBuffer(0)
								   .AddConstantBuffer(1)
								   .AddConstantBuffer(2)
								   .AddDescriptorTables(tables)
								   .AddSampler(0, D3D12_FILTER_ANISOTROPIC)
								   .AddSampler(1, D3D12_FILTER_ANISOTROPIC);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}
}
