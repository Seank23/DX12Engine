#include "GeometryRenderPass.h"
#include "../../Resources/ResourceManager.h"
#include "../RenderContext.h"
#include "../RenderObject.h"

namespace DX12Engine
{
    GeometryRenderPass::GeometryRenderPass(RenderContext& context)
		: RenderPass(context)
    {
    }

    GeometryRenderPass::~GeometryRenderPass()
    {
    }

    void GeometryRenderPass::Init()
    {
		DirectX::XMINT2 windowSize = m_RenderContext.GetWindowSize();
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R8G8B8A8_UNORM)); // Albedo
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R16G16B16A16_FLOAT)); // Normal
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R16G16B16A16_FLOAT)); // Metallic, Roughness, AO
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateRenderTargetTexture(DirectX::XMINT2(windowSize.x, windowSize.y), DXGI_FORMAT_R16G16B16A16_FLOAT)); // Position
		m_RenderTargets.emplace_back(ResourceManager::GetInstance().CreateDepthMap(DirectX::XMINT3(windowSize.x, windowSize.y, 1), DXGI_FORMAT_D32_FLOAT, DXGI_FORMAT_R32_FLOAT, false)); // Depth

        m_Viewport = { 0.0f, 0.0f, (float)windowSize.x, (float)windowSize.y, -1.0f, 1.0f };
        m_ScissorRect = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };

		CreateGeometryPassPSO();
    }

    void GeometryRenderPass::Execute()
    {
        if (!m_RenderContext.GetUploader().UploadAllPending()) // Upload any pending resources
            m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		m_CommandList.SetPipelineState(m_PipelineState.Get());
		m_CommandList.SetGraphicsRootSignature(m_RootSignature.Get());

        m_CommandList.RSSetViewports(1, &m_Viewport);
        m_CommandList.RSSetScissorRects(1, &m_ScissorRect);

		CD3DX12_RESOURCE_BARRIER rtBarriers[5];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[4];
        for (int i = 0; i < 4; i++)
        {
			rtBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
				m_RenderTargets[i]->GetResource(),
                m_RenderTargets[i]->GetUsageState(),
				D3D12_RESOURCE_STATE_RENDER_TARGET
			);
			m_RenderTargets[i]->SetUsageState(D3D12_RESOURCE_STATE_RENDER_TARGET);
            rtvHandles[i] = m_RenderTargets[i]->GetTextureDescriptor().GetCPUHandle();
        }
		rtBarriers[4] = CD3DX12_RESOURCE_BARRIER::Transition(
			m_RenderTargets[4]->GetResource(),
			m_RenderTargets[4]->GetUsageState(),
			D3D12_RESOURCE_STATE_DEPTH_WRITE
		);
        m_RenderTargets[4]->SetUsageState(D3D12_RESOURCE_STATE_DEPTH_WRITE);
        m_CommandList.ResourceBarrier(5, rtBarriers);

		auto dsvHandle = m_RenderTargets[4]->GetTextureDescriptor().GetCPUHandle();
		m_CommandList.OMSetRenderTargets(4, rtvHandles, false, &dsvHandle);

        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		for (int i = 0; i < 4; i++)
			m_CommandList.ClearRenderTargetView(rtvHandles[i], clearColor, 0, nullptr);
		m_CommandList.ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        m_CommandList.IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto srvHeap = m_RenderContext.GetHeapManager().GetRenderPassHeap().GetHeap();
        m_CommandList.SetDescriptorHeaps(1, &srvHeap);

        for (RenderObject* object : m_RenderObjects)
        {
            m_CommandList.SetGraphicsRootConstantBufferView(0, object->GetCBVAddress());
            int startIndex = 1;
			object->GetMaterial()->Bind(&m_CommandList, &startIndex, false);

            auto vertexBufferView = object->GetVertexBufferView();
            auto indexBufferView = object->GetIndexBufferView();
            m_CommandList.IASetVertexBuffers(0, 1, &vertexBufferView);
            m_CommandList.IASetIndexBuffer(&indexBufferView);
            m_CommandList.DrawIndexedInstanced(indexBufferView.SizeInBytes / 4, 1, 0, 0, 0);
        }
        for (int i = 0; i < 5; i++)
        {
            rtBarriers[i] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_RenderTargets[i]->GetResource(),
                m_RenderTargets[i]->GetUsageState(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            m_RenderTargets[i]->SetUsageState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        }
        m_CommandList.ResourceBarrier(5, rtBarriers);

        UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();
        m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
    }

    RenderTexture* GeometryRenderPass::GetRenderTarget(RenderTargetType type)
    {
        switch (type)
        {
		case RenderTargetType::Albedo:
			return m_RenderTargets[0].get();
		case RenderTargetType::Normal:
			return m_RenderTargets[1].get();
		case RenderTargetType::Material:
			return m_RenderTargets[2].get();
		case RenderTargetType::Position:
			return m_RenderTargets[3].get();
        case RenderTargetType::Depth:
            return m_RenderTargets[4].get();
        default:
            return nullptr;
        }
    }

    void GeometryRenderPass::CreateGeometryPassPSO()
    {
        PipelineStateBuilder pipelineStateBuilder;
        RootSignatureBuilder rootSignatureBuilder;

        pipelineStateBuilder = pipelineStateBuilder.AddInputLayout()
            .SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT })
            .SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT)
            .SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
            .SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
            .SetDepthStencilState(CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT))
            .SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
            .SetSampleDesc(UINT_MAX, 1, 0)
            .SetVertexShader(ResourceManager::GetInstance().GetShader("Geometry_VS"))
            .SetPixelShader(ResourceManager::GetInstance().GetShader("Geometry_PS"));

        DescriptorTableConfig config(5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
        rootSignatureBuilder = rootSignatureBuilder.AddConstantBuffer(0)
            .AddConstantBuffer(1)
            .AddDescriptorTables({ config })
            .AddSampler(0, D3D12_FILTER_ANISOTROPIC);

        m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
        pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
        m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
    }
}
