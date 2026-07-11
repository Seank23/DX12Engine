#pragma once
#include <memory>

#include "DX12Engine/Entity/Scene.h"
#include "DX12Engine/Rendering/RenderContext.h"

namespace DX12Engine
{
	class GameObject;
	class PhysicsEngine;
}

namespace DX12EngineDemo
{
	class ComplexDemoScene : public DX12Engine::Scene
	{
	public:
		ComplexDemoScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize);
		~ComplexDemoScene() = default;

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;
		virtual void OnResize(DirectX::XMFLOAT2 newSize) override;
		virtual DX12Engine::GameObject* GetInteractiveObject() override;

	private:
		std::shared_ptr<DX12Engine::RenderContext> m_RenderContext;
		std::shared_ptr<DX12Engine::PhysicsEngine> m_PhysicsEngine;
		DirectX::XMFLOAT2 m_WindowSize;
	};
}
