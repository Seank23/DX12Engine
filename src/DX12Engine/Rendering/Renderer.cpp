#include "Renderer.h"
#include "Heaps/DescriptorHeapHandle.h"
#include "Heaps/DescriptorHeapManager.h"
#include "../Resources/ResourceManager.h"
#include "RenderPass/RenderPass.h"
#include "RenderPass/ShadowMapRenderPass.h"
#include "RenderPass/GeometryRenderPass.h"
#include "RenderPass/LightingRenderPass.h"
#include "RenderPass/TransparentRenderPass.h"
#include "RenderPass/SSRRenderPass.h"
#include "RenderPass/UIRenderPass.h"
#include "RenderPass/TAARenderPass.h"
#include "RenderPipelineConfig.h"
#include "../Entity/GameObject.h"
#include "../Entity/RenderComponent.h"
#include "DrawItem.h"

#include <array>
#include <algorithm>
#include <unordered_set>
#include <cstdint>
#include <cstdio>

namespace DX12Engine
{
	float Renderer::Halton(uint32_t index, uint32_t base)
	{
		float result = 0.0f;
		float fraction = 1.0f;
		while (index > 0)
		{
			fraction /= static_cast<float>(base);
			result += fraction * static_cast<float>(index % base);
			index /= base;
		}
		return result;
	}

	DirectX::XMMATRIX Renderer::UpdateFrameJitter(DirectX::XMMATRIX projectionMatrix, DirectX::XMINT2 screenSize)
	{
		constexpr uint32_t kJitterCycle = 4;

		m_PrevJitter = m_Jitter;
		const uint32_t sampleIndex = static_cast<uint32_t>(m_JitterFrameIndex % kJitterCycle) + 1u;
		m_Jitter.x = Halton(sampleIndex, 2u) - 0.5f;
		m_Jitter.y = Halton(sampleIndex, 3u) - 0.5f;
		++m_JitterFrameIndex;

		const float jxNdc = m_Jitter.x * 2.0f / screenSize.x;
		const float jyNdc = m_Jitter.y * 2.0f / screenSize.y;

		projectionMatrix.r[2].m128_f32[0] += jxNdc;
		projectionMatrix.r[2].m128_f32[1] -= jyNdc;
		return projectionMatrix;
	}

	Renderer::Renderer(std::shared_ptr<RenderContext> context)
		: m_RenderContext(context), m_RenderHeap(context->GetHeapManager().GetRenderPassHeap()), m_QueueManager(context->GetQueueManager()), m_JitteredProjection(DirectX::XMMatrixIdentity())
	{
		m_PostProcessingCB = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(PostProcessingData));
		UpdatePostProcessingCB();

		m_CommandList = m_QueueManager.GetGraphicsQueue().GetCommandList();

		PipelineStateBuilder pipelineStateBuilder;
		RootSignatureBuilder rootSignatureBuilder;

		pipelineStateBuilder = pipelineStateBuilder.SetBlendState(CD3DX12_BLEND_DESC(D3D12_DEFAULT))
			.SetRasterizerState(CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT))
			.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
			.SetRenderTargets({ DXGI_FORMAT_R8G8B8A8_UNORM })
			.SetSampleDesc(UINT_MAX, 1, 0)
			.SetVertexShader(ResourceManager::GetInstance().GetShader("RenderTriangle_VS"))
			.SetPixelShader(ResourceManager::GetInstance().GetShader("FinalRender_PS"));

		DescriptorTableConfig descriptorTable(1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0);
		rootSignatureBuilder = rootSignatureBuilder
			.AddConstantBuffer(0, 0, D3D12_SHADER_VISIBILITY_PIXEL)
			.AddConstantBuffer(1, 0, D3D12_SHADER_VISIBILITY_PIXEL)
			.AddDescriptorTables({descriptorTable})
			.AddSampler(0, D3D12_FILTER_ANISOTROPIC);

		m_RootSignature = ResourceManager::GetInstance().CreateRootSignature(rootSignatureBuilder.Build());
		pipelineStateBuilder = pipelineStateBuilder.SetRootSignature(m_RootSignature.Get());
		m_PipelineState = ResourceManager::GetInstance().CreatePipelineState(pipelineStateBuilder.Build());
	}

	Renderer::~Renderer()
	{
#ifdef _DEBUG
		DescriptorHeapStats stats = m_RenderContext->GetHeapManager().GetStats();
		char buf[256];
		_snprintf_s(buf, sizeof(buf), "[Renderer] Shutdown stats: persistent=%u/%u, transientPeak=%u/%u, failures=%u\n",
			stats.persistentUsed, stats.persistentCapacity,
			stats.transientPeakThisFrame, stats.transientCapacityPerFrame,
			stats.allocationFailures);
		OutputDebugStringA(buf);
#endif
	}

	std::unique_ptr<RenderPass> Renderer::CreateRenderPass(RenderPassType type, int count)
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
		case RenderPassType::Transparent:
			return std::make_unique<TransparentRenderPass>(*m_RenderContext);
		case RenderPassType::ScreenSpaceReflection:
			return std::make_unique<SSRRenderPass>(*m_RenderContext);
		case RenderPassType::TAA:
			if (m_Options.AA_Mode == AntiAliasingMode::TAA)
				return std::make_unique<TAARenderPass>(*m_RenderContext);
			return nullptr;
		case RenderPassType::UI:
			return std::make_unique<UIRenderPass>(*m_RenderContext);
		default:
			return nullptr;
		}
	}

	bool Renderer::PollWindow()
	{
		return m_RenderContext->ProcessWindowMessages();
	}

	void Renderer::ExecutePipeline(RenderPipeline pipeline, float frameTime)
	{
		m_RenderContext->GetHeapManager().BeginFrame(m_FrameIndex++);
		SetSceneData(pipeline, frameTime);
		auto projectionOverride = m_Options.AA_Mode == AntiAliasingMode::TAA ? &m_JitteredProjection : nullptr;
		m_RenderContext->UpdateScreenData(m_CurrentScene ? m_CurrentScene->GetCamera() : nullptr, m_Jitter, m_PrevJitter, projectionOverride);
		m_RenderContext->GetUploader().UploadAllPending();
		for (RenderPass* pass : pipeline.RenderPasses)
		{
			pass->Execute();
		}
		std::shared_ptr<RenderTexture> finalRenderTarget = pipeline.RenderPasses.back()->GetRenderTarget(DX12Engine::ResourceSlot::Composite);
		PresentFrame(finalRenderTarget.get());
	}

	std::unique_ptr<std::vector<ResourceSlot>> Renderer::GetTargets(std::vector<ResourceSlot> targets)
	{
		return std::make_unique<std::vector<ResourceSlot>>(targets);
	}

	RenderPipeline Renderer::CreateRenderPipeline(RenderPipelineConfig config)
	{
		RenderPipeline pipeline;
		std::unordered_map<PipelineResource, RenderPass*> resourceProducers;
		try
		{
			for (RenderPassConfig& passConfig : config.Passes)
			{
				RenderPass* renderPass = CreateRenderPass(passConfig.Type, passConfig.Count).release();
				if (renderPass)
				{
					for (InputResourceType inputType : OrderedInputTypes)
					{
						auto bindingIt = std::find_if(passConfig.ResourceBindings.begin(), passConfig.ResourceBindings.end(),
							[inputType](const ResourceBinding& binding) { return binding.InputType == inputType; });
						if (bindingIt != passConfig.ResourceBindings.end())
						{
							auto producerIt = resourceProducers.find(bindingIt->Resource);
							if (producerIt != resourceProducers.end() && producerIt->second != nullptr)
							{
								for (const ResourceSlot& slot : bindingIt->Slots)
									renderPass->AddInputResources({ producerIt->second->GetRenderTarget(slot) });
								renderPass->AddResourceBlock(bindingIt->InputType, static_cast<UINT>(bindingIt->Slots.size()));
							}
						}

						auto inputIt = passConfig.InputResources.find(inputType);
						if (inputIt == passConfig.InputResources.end())
							continue;

						void* inputResource = inputIt->second;
						switch (inputType)
						{
						case InputResourceType::EnvironmentMap:
							for (auto& texture : *static_cast<std::vector<Texture*>*>(inputResource))
								renderPass->AddInputResources({ std::shared_ptr<Texture>(texture, [](Texture*) {}) });
							renderPass->AddResourceBlock(
								InputResourceType::EnvironmentMap,
								static_cast<UINT>(static_cast<std::vector<Texture*>*>(inputResource)->size())
							);
							break;
						case InputResourceType::VertexShader:
							renderPass->SetVertexShader(*static_cast<std::string*>(inputResource));
							break;
						case InputResourceType::PixelShader:
							renderPass->SetPixelShader(*static_cast<std::string*>(inputResource));
							break;
						default:
							break;
						}
					}

					switch (passConfig.Type)
					{
					case RenderPassType::ShadowMap:
					case RenderPassType::CubeShadowMap:
					{
						auto lightDataIt = passConfig.InputResources.find(InputResourceType::LightData);
						if (lightDataIt != passConfig.InputResources.end())
							static_cast<ShadowMapRenderPass*>(renderPass)->SetLights(*static_cast<std::vector<Light*>*>(lightDataIt->second));
						break;
					}
					case RenderPassType::Geometry:
					case RenderPassType::Lighting:
					case RenderPassType::ScreenSpaceReflection:
					case RenderPassType::UI:
						break;
					}

					renderPass->Init();
					for (ResourceWrite& write : passConfig.Writes)
					{
						resourceProducers[write.Resource] = renderPass;
						write.SourcePass = passConfig.Type;
					}
					pipeline.RenderPasses.push_back(renderPass);
				}
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
		viewport.MinDepth = 0.0f;
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

	void Renderer::SetSceneData(RenderPipeline pipeline, float frameTime)
	{
		std::vector<Texture*> texturesToUpload;
		std::unordered_set<Texture*> queuedTextures;
		Texture* skyboxCubemap = m_CurrentScene->GetSkyboxCubemap();
		if (skyboxCubemap && !skyboxCubemap->GetIsReady())
			m_RenderContext->GetUploader().UploadTextureBatch({ skyboxCubemap, m_CurrentScene->GetSkyboxIrradiance() });
		std::vector<RenderComponent*> renderComponents = m_CurrentScene->GetSceneObjects().GetAllComponents<RenderComponent>();
		Camera* sceneCamera = m_CurrentScene->GetCamera();
		LightBuffer* lightBuffer = m_CurrentScene->GetLightBuffer();

		if (m_Options.AA_Mode == AntiAliasingMode::TAA)
		{
			m_JitteredProjection = UpdateFrameJitter(sceneCamera ? sceneCamera->GetProjectionMatrix() : DirectX::XMMatrixIdentity(), m_RenderContext->GetRenderSize());
			float jitterScale = (1.0f / frameTime) / 120.0f; // scale jitter based on frame time to maintain stability across varying frame rates
			m_Jitter.x *= jitterScale;
			m_Jitter.y *= jitterScale;
		}

		std::vector<DrawItem> geometryPassDrawItems;
		std::vector<DrawItem> transparentPassDrawItems;
		for (auto& comp : renderComponents)
		{
			if (!comp)
				continue;

			for (ResolvedPrimitiveBinding& binding : comp->GetResolvedPrimitiveBindings())
			{
				MaterialTemplate* tmpl = binding.MaterialAsset ? binding.MaterialAsset->GetTemplate() : nullptr;
				Material* material = binding.MaterialAsset ? binding.MaterialAsset->GetMaterial() : nullptr;
				if (!material || !binding.PrimitiveConstantBuffer)
					continue;

				DirectX::XMMATRIX nodeWorldTransform = DirectX::XMLoadFloat4x4(&binding.NodeWorldTransform);
				DirectX::XMMATRIX modelMatrix = nodeWorldTransform * comp->GetModelMatrix();
				DirectX::XMMATRIX unjitteredProjectionMatrix = sceneCamera->GetProjectionMatrix();
				DirectX::XMMATRIX projectionMatrix = (m_Options.AA_Mode == AntiAliasingMode::TAA) ? m_JitteredProjection : unjitteredProjectionMatrix;
				comp->UpdateConstantBufferData(binding, modelMatrix, sceneCamera->GetViewMatrix(), projectionMatrix, unjitteredProjectionMatrix, sceneCamera->GetPosition());

				if (tmpl && !tmpl->HasResolvedPSO())
					tmpl->ResolvePSO();

				DrawItem item{};
				item.Primitive    = binding.Primitive;
				item.Material     = material;
				item.Template     = tmpl;
				item.CBVAddress   = binding.CBVAddress;
				item.IndexCount   = binding.Primitive->GetIndexCount();
				item.FirstIndex   = binding.Primitive->GetFirstIndex();
				item.BaseVertex   = binding.Primitive->GetBaseVertex();
				item.ModelMatrix  = modelMatrix;
				item.PipelineKey  = tmpl ? tmpl->GetPipelineKey() : 0;
				item.MaterialKey  = reinterpret_cast<uint64_t>(material);
				item.MeshKey      = reinterpret_cast<uint64_t>(binding.Primitive);
				item.BlendMode    = tmpl ? tmpl->GetBlendPolicy() : AlphaMode::Opaque;

				if (item.BlendMode == AlphaMode::Blend)
					transparentPassDrawItems.push_back(item);
				else
					geometryPassDrawItems.push_back(item);

				for (int i = 0; i < 5; i++)
				{
					Texture* texture = material->GetTexture(static_cast<TextureType>(i));
					if (!texture || texture->GetIsReady())
						continue;

					auto insertResult = queuedTextures.insert(texture);
					if (insertResult.second)
						texturesToUpload.push_back(texture);
				}
			}
		}
		if (!texturesToUpload.empty())
			m_RenderContext->GetUploader().UploadTextureBatch(texturesToUpload);

		// primary key: PSO variant, secondary: material, tertiary: mesh, quaternary: CBV (object).
		std::sort(geometryPassDrawItems.begin(), geometryPassDrawItems.end(), [](const DrawItem& a, const DrawItem& b)
		{
			if (a.PipelineKey != b.PipelineKey) return a.PipelineKey < b.PipelineKey;
			if (a.MaterialKey  != b.MaterialKey)  return a.MaterialKey  < b.MaterialKey;
			if (a.MeshKey      != b.MeshKey)      return a.MeshKey      < b.MeshKey;
			return a.CBVAddress < b.CBVAddress;
		});

		auto cameraPos = sceneCamera->GetPosition();
		std::sort(transparentPassDrawItems.begin(), transparentPassDrawItems.end(), [cameraPos](const DrawItem& a, const DrawItem& b)
		{
			DirectX::XMVECTOR camVec = DirectX::XMLoadFloat3(&cameraPos);

			DirectX::XMVECTOR posA = a.ModelMatrix.r[3];
			DirectX::XMVECTOR posB = b.ModelMatrix.r[3];

			DirectX::XMVECTOR diffA = DirectX::XMVectorSubtract(posA, camVec);
			DirectX::XMVECTOR diffB = DirectX::XMVectorSubtract(posB, camVec);

			float distSqA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(diffA, diffA));
			float distSqB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(diffB, diffB));

			return distSqA > distSqB; // back-to-front
		});

		for (auto& renderPass : pipeline.RenderPasses)
		{
			renderPass->SetRenderObjects(renderComponents);
			renderPass->SetCamera(sceneCamera);
		switch (renderPass->GetType())
			{
			case RenderPassType::Geometry:
				static_cast<GeometryRenderPass*>(renderPass)->SetDrawItems(geometryPassDrawItems);
				break;
			case RenderPassType::ShadowMap:
			case RenderPassType::CubeShadowMap:
				static_cast<ShadowMapRenderPass*>(renderPass)->SetDrawItems(geometryPassDrawItems);
				break;
			case RenderPassType::Lighting:
				static_cast<LightingRenderPass*>(renderPass)->SetLightBuffer(lightBuffer);
				break;
			case RenderPassType::Transparent:
				static_cast<TransparentRenderPass*>(renderPass)->SetDrawItems(transparentPassDrawItems);
				break;
			case RenderPassType::TAA:
			{
				TAARenderPass* taaPass = static_cast<TAARenderPass*>(renderPass);
				taaPass->SetJitterStates(m_Jitter, m_PrevJitter);
				taaPass->SetTAASettings(m_Options.TAA);
				if (m_RequestTAAHistoryReset)
				{
					taaPass->InvalidateHistory();
				}
				break;
			}
			default:
				break;
			}
		}
	}

	void Renderer::PresentFrame(RenderTexture* finalRenderTarget)
	{
		m_RenderContext->GetUploader().UploadAllPending();
		m_QueueManager.GetGraphicsQueue().ResetCommandAllocatorAndList();

		std::vector<GPUResource*> finalTargetVec = { finalRenderTarget };
		ResourceManager::GetInstance().UpdateSRVDescriptors(finalTargetVec);

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

		m_CommandList->SetGraphicsRootConstantBufferView(0, m_RenderContext->GetScreenDataBuffer().GetGPUAddress());
		m_CommandList->SetGraphicsRootConstantBufferView(1, m_PostProcessingCB->GetGPUAddress());
		m_CommandList->SetGraphicsRootDescriptorTable(2, finalRenderTarget->GetTransientDescriptor()->GetGPUHandle());

		m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_CommandList->DrawInstanced(3, 1, 0, 0);

		barrier = m_RenderContext->TransitionRenderTarget(false);
		m_CommandList->ResourceBarrier(1, &barrier);

		UINT fenceVal = m_QueueManager.GetGraphicsQueue().ExecuteCommandList();

		m_RenderContext->PresentFrame();
		m_QueueManager.GetGraphicsQueue().WaitForFenceCPUBlocking(fenceVal);

#ifdef _DEBUG
		if (m_FrameIndex % 60 == 0)
		{
			DescriptorHeapStats stats = m_RenderContext->GetHeapManager().GetStats();
			char buf[256];
			_snprintf_s(buf, sizeof(buf),
				"[Renderer] Frame %u: persistent=%u/%u (%.0f%%), transient=%u/%u (%.0f%%), failures=%u\n",
				m_FrameIndex,
				stats.persistentUsed, stats.persistentCapacity,
				100.0f * stats.persistentUsed / (stats.persistentCapacity ? stats.persistentCapacity : 1),
				stats.transientUsedThisFrame, stats.transientCapacityPerFrame,
				100.0f * stats.transientUsedThisFrame / (stats.transientCapacityPerFrame ? stats.transientCapacityPerFrame : 1),
				stats.allocationFailures);
			OutputDebugStringA(buf);
			if (stats.persistentUsed > static_cast<UINT>(stats.persistentCapacity * HEAP_WARN_THRESHOLD_HIGH))
				OutputDebugStringA("[Renderer] WARNING: Persistent SRV heap above 95% capacity!\n");
			else if (stats.persistentUsed > static_cast<UINT>(stats.persistentCapacity * HEAP_WARN_THRESHOLD_MED))
				OutputDebugStringA("[Renderer] WARNING: Persistent SRV heap above 80% capacity.\n");
		}
#endif
	}

	void Renderer::SetOptions(RendererOptions options)
	{
		const bool aaModeChanged = m_Options.AA_Mode != options.AA_Mode;
		const bool renderScaleChanged = std::fabs(m_Options.RenderScale - options.RenderScale) > 0.001f;
		const bool taaSettingsChanged =
			m_Options.TAA.BaseBlend != options.TAA.BaseBlend ||
			m_Options.TAA.MinBlend != options.TAA.MinBlend ||
			m_Options.TAA.MaxBlend != options.TAA.MaxBlend ||
			m_Options.TAA.VelocityRejection != options.TAA.VelocityRejection ||
			m_Options.TAA.DepthRejection != options.TAA.DepthRejection ||
			m_Options.TAA.ClampGamma != options.TAA.ClampGamma ||
			m_Options.TAA.Sharpness != options.TAA.Sharpness ||
			m_Options.TAA.DisocclusionDepthThreshold != options.TAA.DisocclusionDepthThreshold;

		m_Options = options;
		m_RenderContext->SetRenderScale(m_Options.RenderScale);
		if (aaModeChanged || taaSettingsChanged || renderScaleChanged)
		{
			m_RequestTAAHistoryReset = true;
		}
		UpdatePostProcessingCB();
	}

	void Renderer::UpdatePostProcessingCB()
	{
		PostProcessingData data{};
		data.EnableGammaCorrection = m_Options.EnableGammaCorrection ? 1 : 0;
		data.EnableFXAA = m_Options.AA_Mode == AntiAliasingMode::FXAA ? 1 : 0;
		m_PostProcessingCB->Update(&data, sizeof(PostProcessingData));
	}
}
