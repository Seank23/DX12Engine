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
	class DemoScene : public DX12Engine::Scene
	{
	public:
		DemoScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize);
		~DemoScene() = default;

		void Init() override;
		void Update(float ts, float elapsed) override;
		void OnResize(DirectX::XMFLOAT2 newSize);

	private:
		std::shared_ptr<DX12Engine::RenderContext> m_RenderContext;
		std::shared_ptr<DX12Engine::PhysicsEngine> m_PhysicsEngine;
		DirectX::XMFLOAT2 m_WindowSize;
		std::shared_ptr<DX12Engine::GameObject> m_Cube;
		std::shared_ptr<DX12Engine::GameObject> m_Ball;
	};
}
