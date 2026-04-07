#include "DX12EngineDemoApp.h"

#include <windowsx.h>

#include "DemoScene.h"
#include "ComplexDemoScene.h"
#include "DX12Engine/Rendering/RenderPipelineConfig.h"
#include "DX12Engine/Rendering/PipelineBuilder.h"

namespace DX12EngineDemo
{
	DX12EngineDemoApp::DX12EngineDemoApp()
		: Application()
	{
	}

	void DX12EngineDemoApp::Init(std::shared_ptr<DX12Engine::RenderContext> renderContext, DirectX::XMFLOAT2 windowSize)
	{
		m_RenderContext = renderContext;
		m_Renderer = std::make_unique<DX12Engine::Renderer>(m_RenderContext);
		DX12Engine::RendererOptions options;
		options.AA_Mode = DX12Engine::AntiAliasingMode::None;
		m_Renderer->SetOptions(options);

		m_PhysicsEngine = std::make_shared<DX12Engine::PhysicsEngine>();

		m_Scene = std::make_unique<ComplexDemoScene>(m_RenderContext, m_PhysicsEngine, windowSize);
		m_Scene->Init();
		m_Renderer->SetCurrentScene(m_Scene.get());

		m_InputHandler = std::make_unique<DemoInputHandler>();
		m_InputHandler->SetCamera(m_Scene->GetCamera());

		auto shadowCastingLights = m_Scene->GetLightBuffer()->GetLightsByType({ DX12Engine::LightType::Directional, DX12Engine::LightType::Spot });
		auto cubeShadowCastingLights = m_Scene->GetLightBuffer()->GetLightsByType({ DX12Engine::LightType::Point });

		DX12Engine::RenderPassConfig shadowMapConfig;
		shadowMapConfig.Type = DX12Engine::RenderPassType::ShadowMap;
		shadowMapConfig.Count = static_cast<int>(shadowCastingLights.size());
		shadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &shadowCastingLights;
		shadowMapConfig.Writes.push_back({ DX12Engine::PipelineResource::ShadowMap });
		std::vector<DX12Engine::ResourceSlot> shadowBufferTypes{
			DX12Engine::ResourceSlot::Depth
		};

		DX12Engine::RenderPassConfig cubeShadowMapConfig;
		cubeShadowMapConfig.Type = DX12Engine::RenderPassType::CubeShadowMap;
		cubeShadowMapConfig.Count = static_cast<int>(cubeShadowCastingLights.size());
		cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &cubeShadowCastingLights;
		cubeShadowMapConfig.Writes.push_back({ DX12Engine::PipelineResource::CubeShadowMap });
		std::vector<DX12Engine::ResourceSlot> cubeShadowBufferTypes{
			DX12Engine::ResourceSlot::Depth
		};

		DX12Engine::RenderPassConfig geometryConfig;
		geometryConfig.Type = DX12Engine::RenderPassType::Geometry;
		geometryConfig.Writes.push_back({ DX12Engine::PipelineResource::GBuffer });
		geometryConfig.Writes.push_back({ DX12Engine::PipelineResource::Depth });
		std::vector<DX12Engine::ResourceSlot> gBufferTypes{
			DX12Engine::ResourceSlot::Albedo,
			DX12Engine::ResourceSlot::WorldNormal,
			DX12Engine::ResourceSlot::ObjectNormal,
			DX12Engine::ResourceSlot::Material,
			DX12Engine::ResourceSlot::Position,
			DX12Engine::ResourceSlot::Emissive,
			DX12Engine::ResourceSlot::Depth
		};

		std::vector<DX12Engine::Texture*> enviroAndIrradiance{ m_Scene->GetSkyboxCubemap(), m_Scene->GetSkyboxIrradiance() };
		std::vector<DX12Engine::Texture*> enviro{ m_Scene->GetSkyboxCubemap() };
		DX12Engine::RenderPassConfig lightingConfig;
		lightingConfig.Type = DX12Engine::RenderPassType::Lighting;
		lightingConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviroAndIrradiance;
		lightingConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::ShadowMap, DX12Engine::PipelineResource::ShadowMap, shadowBufferTypes });
		lightingConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::CubeShadowMap, DX12Engine::PipelineResource::CubeShadowMap, cubeShadowBufferTypes });
		lightingConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::GBuffer, DX12Engine::PipelineResource::GBuffer, gBufferTypes });
		lightingConfig.Writes.push_back({ DX12Engine::PipelineResource::SceneColor });

		std::vector<DX12Engine::ResourceSlot> compositeType{ DX12Engine::ResourceSlot::Composite };
		std::vector<DX12Engine::ResourceSlot> ssrGBufferTypes{
			DX12Engine::ResourceSlot::Albedo,
			DX12Engine::ResourceSlot::WorldNormal,
			DX12Engine::ResourceSlot::Material,
			DX12Engine::ResourceSlot::Position,
			DX12Engine::ResourceSlot::Depth
		};
		DX12Engine::RenderPassConfig ssrConfig;
		ssrConfig.Type = DX12Engine::RenderPassType::ScreenSpaceReflection;
		ssrConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviro;
		ssrConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::GBuffer, DX12Engine::PipelineResource::GBuffer, ssrGBufferTypes });
		ssrConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::SceneColor, DX12Engine::PipelineResource::SceneColor, compositeType });
		ssrConfig.Writes.push_back({ DX12Engine::PipelineResource::SceneColor });

		DX12Engine::RenderPassConfig taaConfig;
		taaConfig.Type = DX12Engine::RenderPassType::TAA;
		taaConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::SceneColor, DX12Engine::PipelineResource::SceneColor, compositeType });
		taaConfig.Writes.push_back({ DX12Engine::PipelineResource::SceneColor });

		std::vector<DX12Engine::ResourceSlot> transparentDepthTypes{ DX12Engine::ResourceSlot::Depth };
		DX12Engine::RenderPassConfig transparentConfig;
		transparentConfig.Type = DX12Engine::RenderPassType::Transparent;
		transparentConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviro;
		transparentConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::Depth, DX12Engine::PipelineResource::Depth, transparentDepthTypes });
		transparentConfig.ResourceBindings.push_back({ DX12Engine::InputResourceType::SceneColor, DX12Engine::PipelineResource::SceneColor, compositeType });
		transparentConfig.Writes.push_back({ DX12Engine::PipelineResource::SceneColor });

		DX12Engine::PipelineBuilder builder;
		builder.AddPass(shadowMapConfig)
			.AddPass(cubeShadowMapConfig)
			.AddPass(geometryConfig)
			.AddPass(lightingConfig)
			.AddPass(ssrConfig)
			.AddPassIf(options.AA_Mode == DX12Engine::AntiAliasingMode::TAA, taaConfig)
			.AddPass(transparentConfig);

		m_RenderPipeline = m_Renderer->CreateRenderPipeline(builder.Build());
	}

	void DX12EngineDemoApp::Update(float ts, float elapsed)
	{
		m_InputHandler->ProcessInput(ts);
		m_PhysicsEngine->Update(ts, elapsed);
		m_Scene->Update(ts, elapsed);
		m_Scene->GetLightBuffer()->Update();
		m_Renderer->ExecutePipeline(m_RenderPipeline);
	}

	void DX12EngineDemoApp::HandleWindowEvent(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_SIZE:
			OnResize({ static_cast<float>(LOWORD(lParam)), static_cast<float>(HIWORD(lParam)) });
			break;
		case WM_MOUSEMOVE:
			m_InputHandler->HandleMouseMovement(hwnd, lParam);
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			m_InputHandler->HandleMouseClick(hwnd, lParam);
			break;
		case WM_MOUSEWHEEL:
			m_InputHandler->HandleMouseWheel(hwnd, wParam);
			break;
		}
	}

	void DX12EngineDemoApp::OnResize(DirectX::XMFLOAT2 newSize)
	{
		if (m_RenderContext)
			m_RenderContext->SetWindowSize({ static_cast<int>(newSize.x), static_cast<int>(newSize.y) });
		if (m_Scene)
			m_Scene->OnResize(newSize);
	}
}
