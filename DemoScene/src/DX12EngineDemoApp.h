#pragma once

#include <memory>

#include "DX12Engine/Application.h"
#include "DX12Engine/Rendering/RenderContext.h"
#include "DX12Engine/Rendering/Renderer.h"
#include "DX12Engine/Physics/PhysicsEngine.h"
#include "DX12Engine/Entity/Scene.h"
#include "Input/DemoInputHandler.h"

namespace DX12EngineDemo
{
	class DX12EngineDemoApp : public DX12Engine::Application
	{
	public:
		DX12EngineDemoApp();
		~DX12EngineDemoApp() = default;

		void Init(std::shared_ptr<DX12Engine::RenderContext> renderContext, DirectX::XMFLOAT2 windowSize) override;
		void Update(float ts, float elapsed) override;
		void HandleWindowEvent(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

	private:
		void OnResize(DirectX::XMFLOAT2 newSize);

		std::unique_ptr<DemoInputHandler> m_InputHandler;
		std::unique_ptr<DX12Engine::Scene> m_Scene;
		std::unique_ptr<DX12Engine::Renderer> m_Renderer;
		DX12Engine::RenderPipeline m_RenderPipeline;

		std::shared_ptr<DX12Engine::PhysicsEngine> m_PhysicsEngine;
	};
}
