#include "Renderer.h"
#include "../Heaps/DescriptorHeapHandle.h"
#include "../Heaps/DescriptorHeapManager.h"
#include "../Resources/ResourceManager.h"
#include "RenderPass/RenderPass.h"
#include "RenderPass/ShadowMapRenderPass.h"
#include "RenderPass/GeometryRenderPass.h"
#include "RenderPass/LightingRenderPass.h"
#include "RenderPass/SSRRenderPass.h"
#include "RenderPipelineConfig.h"

namespace DX12Engine
{
	Renderer::Renderer(std::shared_ptr<RenderContext> context)
		: m_RenderContext(context), m_RenderHeap(context->GetHeapManager().GetRenderPassHeap()), m_QueueManager(context->GetQueueManager())
	{
		m_CommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();

		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0).SetVertexShader(ResourceManager::GetInstance().GetShader("RenderTriangle_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("FinalRender_PS"));

		DescriptorTableConfig descriptorTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		rootSignatureBuilder = rootSignatureBuilder.AddDescriptorTables({ descriptorTable }).AddSampler(0, D3D12_FILTER_ANISOTROPIC);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::PresentFrame(RenderTexture* finalRenderTarget)
	{
		if (!m_RenderContext->GetUploader().UploadAllPending()) // Upload any pending resources
			m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		m_CommandList->SetPipelineState(m_PipelineState.Get());
		m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());

		auto viewport = GetDefaultViewport();
		auto scissorRect = GetDefaultScissorRect();
		m_CommandList->RSSetViewports(1, &viewport);
		m_CommandList->RSSetScissorRects(1, &scissorRect);

		auto barrier = m_RenderContext->TransitionRenderTarget(true);
		m_CommandList->ResourceBarrier(1, &barrier);

		auto rtvHandle = m_RenderContext->GetRTVHandle();
		m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
		m_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		auto srvHeap = m_RenderHeap.GetHeap();
		m_CommandList->SetDescriptorHeaps(1, &srvHeap);

		m_CommandList->SetGraphicsRootDescriptorTable(0, finalRenderTarget->GetDescriptor()->GetGPUHandle());

		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList->DrawInstanced(3, 1, 0, 0);

		barrier = m_RenderContext->TransitionRenderTarget(false);
		m_CommandList->ResourceBarrier(1, &barrier);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();

		m_RenderContext->PresentFrame();
		m_QueueManager.WaitForFenceCPUBlocking(fenceVal);
	}

	std::unique_ptr<RenderPass> Renderer::GetRenderPass(RenderPassType type, int count)
	{
		switch (type)
		{
		case RenderPassType::ShadowMap:
			return std::make_unique<ShadowMapRenderPass>(*m_RenderContext, count, false);
		case RenderPassType::CubeShadowMap:
			return std::make_unique<ShadowMapRenderPass>(*m_RenderContext, count, true);
		case RenderPassType::Geometry:
			return std::make_unique<GeometryRenderPass>(*m_RenderContext);
		case RenderPassType::Lighting:
			return std::make_unique<LightingRenderPass>(*m_RenderContext);
		case RenderPassType::ScreenSpaceReflection:
			return std::make_unique<SSRRenderPass>(*m_RenderContext);
		default:
			return nullptr;
		}
	}

	bool Renderer::PollWindow()
	{
		return m_RenderContext->ProcessWindowMessages();
	}

	void Renderer::UpdateObjectList(std::vector<RenderObject*> objects)
	{
		for (RenderObject* obj : objects)
		{
			obj->UpdateConstantBufferData(m_Camera->GetViewMatrix(), m_Camera->GetProjectionMatrix(), m_Camera->GetPosition());
		}
	}

	void Renderer::ExecutePipeline(RenderPipeline pipeline)
	{
		for (RenderPass* pass : pipeline.RenderPasses)
		{
			pass->Execute();
		}
		RenderTexture* finalRenderTarget = pipeline.RenderPasses.back()->GetRenderTarget(DX12Engine::RenderTargetType::Composite);
		PresentFrame(finalRenderTarget);
	}

	std::unique_ptr<std::vector<RenderTargetType>> Renderer::GetTargets(std::vector<RenderTargetType> targets)
	{
		return std::make_unique<std::vector<RenderTargetType>>(targets);
	}

	RenderPipeline Renderer::CreateRenderPipeline(RenderPipelineConfig config)
	{
		RenderPipeline pipeline;
		std::unordered_map<RenderPassType, int> renderPassOrder;
		int i = 0;
		try
		{
			for (const RenderPassConfig& passConfig : config.Passes)
			{
				RenderPass* renderPass = GetRenderPass(passConfig.Type, passConfig.Count).release();
				renderPassOrder[passConfig.Type] = i;
				if (renderPass)
				{
					// Common input resources
					for (auto& inputResource : passConfig.InputResources)
					{
						switch (inputResource.first)
						{
						case InputResourceType::SceneObjects:
							renderPass->SetRenderObjects(*static_cast<std::vector<RenderObject*>*>(inputResource.second));
							break;
						case InputResourceType::RenderTargets_ShadowMap:
							
							for (auto& target : *static_cast<std::vector<RenderTargetType>*>(inputResource.second))
							{
								renderPass->AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, renderPass->GetInputResourceCount() });
								renderPass->AddInputResources({ pipeline.RenderPasses[renderPassOrder[RenderPassType::ShadowMap]]->GetRenderTarget(target) });
							}
							break;
						case InputResourceType::RenderTargets_CubeShadowMap:
							for (auto& target : *static_cast<std::vector<RenderTargetType>*>(inputResource.second))
							{
								renderPass->AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, renderPass->GetInputResourceCount() });
								renderPass->AddInputResources({ pipeline.RenderPasses[renderPassOrder[RenderPassType::CubeShadowMap]]->GetRenderTarget(target) });
							}
							break;
						case InputResourceType::RenderTargets_Geometry:
							for (auto& target : *static_cast<std::vector<RenderTargetType>*>(inputResource.second))
							{
								renderPass->AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, renderPass->GetInputResourceCount() });
								renderPass->AddInputResources({ pipeline.RenderPasses[renderPassOrder[RenderPassType::Geometry]]->GetRenderTarget(target) });
							}
							break;
						case InputResourceType::RenderTargets_Lighting:
							for (auto& target : *static_cast<std::vector<RenderTargetType>*>(inputResource.second))
							{
								renderPass->AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, renderPass->GetInputResourceCount() });
								renderPass->AddInputResources({ pipeline.RenderPasses[renderPassOrder[RenderPassType::Lighting]]->GetRenderTarget(target) });
							}
							break;
						case InputResourceType::ExternalTextures:
							for (auto& texture : *static_cast<std::vector<Texture*>*>(inputResource.second))
							{
								renderPass->AddDescriptorTableConfig({ 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, renderPass->GetInputResourceCount() });
								renderPass->AddInputResources({ texture });
							}
							break;
						}
					}
					// Pass-specific input resources
					switch (passConfig.Type)
					{
					case RenderPassType::ShadowMap:
					case RenderPassType::CubeShadowMap:
						for (auto& inputResource : passConfig.InputResources)
						{
							switch (inputResource.first)
							{

							case InputResourceType::LightData:
								static_cast<ShadowMapRenderPass*>(renderPass)->SetLights(*static_cast<std::vector<Light*>*>(inputResource.second));
								break;
							}
						}
						break;
					case RenderPassType::Geometry:
						break;
					case RenderPassType::Lighting:
						for (auto& inputResource : passConfig.InputResources)
						{
							switch (inputResource.first)
							{
							case InputResourceType::LightBuffer:
								static_cast<LightingRenderPass*>(renderPass)->SetLightBuffer(static_cast<LightBuffer*>(inputResource.second));
								break;
							case InputResourceType::Camera:
								static_cast<LightingRenderPass*>(renderPass)->SetCamera(static_cast<Camera*>(inputResource.second));
								break;
							}
						}
						break;
					case RenderPassType::ScreenSpaceReflection:
						for (auto& inputResource : passConfig.InputResources)
						{
							switch (inputResource.first)
							{
							case InputResourceType::Camera:
								static_cast<SSRRenderPass*>(renderPass)->SetCamera(static_cast<Camera*>(inputResource.second));
								break;
							}
						}
						break;
					}
					renderPass->Init();
					pipeline.RenderPasses.push_back(renderPass);
				}
				i++;
			}
		}
		catch (const std::exception& e)
		{
			for (RenderPass* pass : pipeline.RenderPasses)
			{
				delete pass;
			}
			pipeline.RenderPasses.clear();
			throw std::runtime_error("Failed to create render pipeline: " + std::string(e.what()));
		}
		return pipeline;
	}

	D3D12_VIEWPORT Renderer::GetDefaultViewport()
	{
		DirectX::XMINT2 windowSize = m_RenderContext->GetWindowSize();
		D3D12_VIEWPORT viewport{};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = static_cast<float>(windowSize.x);
		viewport.Height = static_cast<float>(windowSize.y);
		viewport.MinDepth = -1.0f;
		viewport.MaxDepth = 1.0f;
		return viewport;
	}

	D3D12_RECT Renderer::GetDefaultScissorRect()
	{
		DirectX::XMINT2 windowSize = m_RenderContext->GetWindowSize();
		D3D12_RECT scissorRect{};
		scissorRect.left = 0;
		scissorRect.top = 0;
		scissorRect.right = static_cast<LONG>(windowSize.x);
		scissorRect.bottom = static_cast<LONG>(windowSize.y);
		return scissorRect;
	}
}
