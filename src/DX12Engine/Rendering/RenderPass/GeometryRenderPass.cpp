#include "GeometryRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../../Entity/RenderComponent.h"
#include "../../Asset/MaterialTemplate.h"
#include "../../Asset/MeshPrimitive.h"
#include "../RenderUtils.h"

namespace DX12Engine
{
    GeometryRenderPass::GeometryRenderPass(RenderContext& context)
		: RenderPass(context)
    {
		m_Type = RenderPassType::Geometry;
    }

    GeometryRenderPass::~GeometryRenderPass()
    {
    }

    void GeometryRenderPass::Init()
    {
        RenderPass::Init();

		m_VertexShaderName = m_VertexShaderName.empty() ? "Geometry_VS" : m_VertexShaderName;
		m_PixelShaderName = m_PixelShaderName.empty() ? "Geometry_PS" : m_PixelShaderName;

		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R8G8B8A8_UNORM, 1 , { 1.0f, 1.0f, 1.0f, 1.0f })); // Albedo
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT)); // World Normal
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT)); // Object Normal
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT)); // Metallic, Roughness, AO
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT)); // Position
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(windowSize, DXGI_FORMAT_R16G16B16A16_FLOAT)); // Emissive
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateDepthMap(DirectX::XMINT3(windowSize.x, windowSize.y, 1), DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, false)); // Depth

        m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, 1.0f };
        m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		CreateGeometryPassPSO();
    }

    void GeometryRenderPass::Execute()
    {
		RenderPass::Execute();

		RenderUtils::UpdateMaterialBindings(m_DrawItems);

        m_CommandList.RSSetViewports(1, &m_Viewport);
        m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		CD3DX12_RESOURCE_BARRIER rtBarriers[7];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[6];
        for (int i = 0; i < 6; i++)
        {
			rtBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[i]->GetResource(),
                m_RenderTargets[i]->GetUsageState(),
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			m_RenderTargets[i]->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtvHandles[i] = m_RenderTargets[i]->GetTextureDescriptor().GetCPUHandle();
        }
		rtBarriers[6] = CD3DX12_RESOURCE_BARRIER::Transition(
			m_RenderTargets[6]->GetResource(),
			m_RenderTargets[6]->GetUsageState(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
        m_RenderTargets[6]->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_CommandList.ResourceBarrier(m_RenderTargets.size(), rtBarriers);

		auto dsvHandle = m_RenderTargets[6]->GetTextureDescriptor().GetCPUHandle();
		m_CommandList.OMSetRenderTargets(6, rtvHandles, false, &dsvHandle);
        for (int i = 0; i < 6; i++)
        {
			DirectX::XMFLOAT4 rtClearColor = m_RenderTargets[i]->GetClearColor();
            const float clearColor[] = { rtClearColor.x, rtClearColor.y, rtClearColor.z, rtClearColor.w };
            m_CommandList.ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
        }
		m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
        m_CommandList.SetDescriptorHeaps(1, &srvHeap);

		D3D12_GPU_VIRTUAL_ADDRESS lastCBV = 0;
		Material* lastMaterial = nullptr;
		MeshPrimitive* lastPrimitive = nullptr;
        uint64_t lastPipelineKey = -1;

        for (const DrawItem& item : m_DrawItems)
        {
            if (!item.Primitive || !item.Material)
                continue;

            if (item.BlendMode == AlphaMode::Blend)
                continue;

            MaterialTemplate* tmpl = item.Template;
            const bool hasTemplatePSO = tmpl && tmpl->GetPassTarget() == PassTarget::Geometry && tmpl->HasResolvedPSO();

            if (!hasTemplatePSO)
            {
                m_CommandList.SetPipelineState(m_PipelineState.Get());
                m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());
                lastCBV = 0;
                lastMaterial = nullptr;
                lastPipelineKey = UINT64_MAX;
                lastPrimitive = nullptr;
            }
            else if (item.PipelineKey != lastPipelineKey)
            {
                m_CommandList.SetPipelineState(tmpl->GetPipelineState());
                m_CommandList.SetGraphicsRootSignature(tmpl->GetRootSignature());
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
                int startIndex = 1;
                item.Material->Bind(&m_CommandList, &startIndex);
                lastMaterial = item.Material;
            }

            if (item.Primitive != lastPrimitive)
            {
                auto vertexBufferView = item.Primitive->GetVertexBufferView();
                auto indexBufferView  = item.Primitive->GetIndexBufferView();
                m_CommandList.IASetVertexBuffers(0, 1, &vertexBufferView);
                m_CommandList.IASetIndexBuffer(&indexBufferView);
                lastPrimitive = item.Primitive;
            }

            m_CommandList.DrawIndexedInstanced(item.IndexCount, 1, item.FirstIndex, item.BaseVertex, 0);
        }
        for (int i = 0; i < m_RenderTargets.size(); i++)
        {
            rtBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_RenderTargets[i]->GetResource(),
                m_RenderTargets[i]->GetUsageState(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            m_RenderTargets[i]->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        m_CommandList.ResourceBarrier(static_cast<UINT>(m_RenderTargets.size()), rtBarriers);

        UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
        m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(fenceVal);
    }

    RenderTexture* GeometryRenderPass::GetRenderTarget(RenderTargetType type)
    {
        switch (type)
        {
		case RenderTargetType::Albedo:
			return m_RenderTargets[0].get();
		case RenderTargetType::WorldNormal:
			return m_RenderTargets[1].get();
        case RenderTargetType::ObjectNormal:
            return m_RenderTargets[2].get();
		case RenderTargetType::Material:
			return m_RenderTargets[3].get();
		case RenderTargetType::Position:
			return m_RenderTargets[4].get();
        case RenderTargetType::Emissive:
            return m_RenderTargets[5].get();
        case RenderTargetType::Depth:
            return m_RenderTargets[6].get();
        default:
            return nullptr;
        }
    }

    void GeometryRenderPass::CreateGeometryPassPSO()
    {
        PipelineStateBuilder pipelineStateBuilder;
        RootSignatureBuilder rootSignatureBuilder;

        pipelineStateBuilder = pipelineStateBuilder
            .ConfigureFromDefault(ResourceManager::GetInstance().GetShader(m_VertexShaderName), ResourceManager::GetInstance().GetShader(m_PixelShaderName))
            .SetRenderTargets({
                DXGI_FORMAT_R8G8B8A8_UNORM,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT,
                DXGI_FORMAT_R16G16B16A16_FLOAT })
            .SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT);

        DescriptorTableConfig config(6, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
        rootSignatureBuilder = rootSignatureBuilder
            .AddConstantBuffer(0)
            .AddConstantBuffer(1)
            .AddDescriptorTables({ config })
            .AddSampler(0, D3D12_FILTER_ANISOTROPIC);

        m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
        pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
        m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
    }
}
