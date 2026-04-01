#include "DX12EngineDemoApp.h"

#include <windowsx.h>

#include "DemoScene.h"
#include "ComplexDemoScene.h"
#include "DX12Engine/Rendering/RenderPipelineConfig.h"

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

		m_PhysicsEngine = std::make_shared<DX12Engine::PhysicsEngine>();

		m_Scene = std::make_unique<ComplexDemoScene>(m_RenderContext, m_PhysicsEngine, windowSize);
		m_Scene->Init();
		m_Renderer->SetCurrentScene(m_Scene.get());

		m_InputHandler = std::make_unique<DemoInputHandler>();
		m_InputHandler->SetCamera(m_Scene->GetCamera());

		auto shadowCastingLights = m_Scene->GetLightBuffer()->GetLightsByType({ DX12Engine::LightType::Directional, DX12Engine::LightType::Spot });
		auto cubeShadowCastingLights = m_Scene->GetLightBuffer()->GetLightsByType({ DX12Engine::LightType::Point });

		DX12Engine::RenderPipelineConfig pipelineConfig;

		DX12Engine::RenderPassConfig shadowMapConfig;
		shadowMapConfig.Type = DX12Engine::RenderPassType::ShadowMap;
		shadowMapConfig.Count = static_cast<int>(shadowCastingLights.size());
		shadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &shadowCastingLights;
		std::vector<DX12Engine::RenderTargetType> shadowBufferTypes{
			DX12Engine::RenderTargetType::Depth
		};

		DX12Engine::RenderPassConfig cubeShadowMapConfig;
		cubeShadowMapConfig.Type = DX12Engine::RenderPassType::CubeShadowMap;
		cubeShadowMapConfig.Count = static_cast<int>(cubeShadowCastingLights.size());
		cubeShadowMapConfig.InputResources[DX12Engine::InputResourceType::LightData] = &cubeShadowCastingLights;
		std::vector<DX12Engine::RenderTargetType> cubeShadowBufferTypes{
			DX12Engine::RenderTargetType::Depth
		};

		DX12Engine::RenderPassConfig geometryConfig;
		geometryConfig.Type = DX12Engine::RenderPassType::Geometry;
		std::vector<DX12Engine::RenderTargetType> gBufferTypes{
			DX12Engine::RenderTargetType::Albedo,
			DX12Engine::RenderTargetType::WorldNormal,
			DX12Engine::RenderTargetType::ObjectNormal,
			DX12Engine::RenderTargetType::Material,
			DX12Engine::RenderTargetType::Position,
			DX12Engine::RenderTargetType::Emissive,
			DX12Engine::RenderTargetType::Depth
		};

		std::vector<DX12Engine::Texture*> enviroAndIrradiance{
			m_Scene->GetSkyboxCubemap(),
			m_Scene->GetSkyboxIrradiance()
		};
		std::vector<DX12Engine::Texture*> enviro{ m_Scene->GetSkyboxCubemap() };
		DX12Engine::RenderPassConfig lightingConfig;
		lightingConfig.Type = DX12Engine::RenderPassType::Lighting;
		lightingConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviroAndIrradiance;
		lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Geometry] = &gBufferTypes;
		lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_ShadowMap] = &shadowBufferTypes;
		lightingConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_CubeShadowMap] = &cubeShadowBufferTypes;

		std::vector<DX12Engine::RenderTargetType> compositeType{ DX12Engine::RenderTargetType::Composite };
		std::vector<DX12Engine::RenderTargetType> ssrGBufferTypes{
			DX12Engine::RenderTargetType::Albedo,
			DX12Engine::RenderTargetType::WorldNormal,
			DX12Engine::RenderTargetType::Material,
			DX12Engine::RenderTargetType::Position,
			DX12Engine::RenderTargetType::Depth
		};
		DX12Engine::RenderPassConfig ssrConfig;
		ssrConfig.Type = DX12Engine::RenderPassType::ScreenSpaceReflection;
		ssrConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviro;
		ssrConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Geometry] = &ssrGBufferTypes;
		ssrConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Lighting] = &compositeType;

		std::vector<DX12Engine::RenderTargetType> transparentDepthTypes{ DX12Engine::RenderTargetType::Depth };
		DX12Engine::RenderPassConfig transparentConfig;
		transparentConfig.Type = DX12Engine::RenderPassType::Transparent;
		transparentConfig.InputResources[DX12Engine::InputResourceType::EnvironmentMap] = &enviro;
		transparentConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_Geometry] = &transparentDepthTypes;
		transparentConfig.InputResources[DX12Engine::InputResourceType::RenderTargets_SSR] = &compositeType;

		pipelineConfig.Passes.push_back(shadowMapConfig);
		pipelineConfig.Passes.push_back(cubeShadowMapConfig);
		pipelineConfig.Passes.push_back(geometryConfig);
		pipelineConfig.Passes.push_back(lightingConfig);
		pipelineConfig.Passes.push_back(ssrConfig);
		pipelineConfig.Passes.push_back(transparentConfig);
		m_RenderPipeline = m_Renderer->CreateRenderPipeline(pipelineConfig);
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
