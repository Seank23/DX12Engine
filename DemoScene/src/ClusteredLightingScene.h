#pragma once
#include <memory>
#include <string>
#include <vector>

#include "DX12Engine/Entity/Scene.h"
#include "DX12Engine/Rendering/RenderContext.h"

namespace DX12Engine
{
	class GameObject;
	class Light;
	class ModelAsset;
	class PhysicsEngine;
}

namespace DX12EngineDemo
{
	// A hall lit by a few hundred small local lights, built to load the froxel grid rather
	// than the mesh pipeline: enough depth to reach the outer Z slices, enough lateral
	// spread that neighbouring clusters hold genuinely different light sets, and a
	// deliberate pile-up that drives one region far past the average occupancy the flat
	// light-index list is budgeted for.
	class ClusteredLightingScene : public DX12Engine::Scene
	{
	public:
		ClusteredLightingScene(std::shared_ptr<DX12Engine::RenderContext> renderContext, std::shared_ptr<DX12Engine::PhysicsEngine> physicsEngine, DirectX::XMFLOAT2 windowSize);
		~ClusteredLightingScene() = default;

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;
		virtual void OnResize(DirectX::XMFLOAT2 newSize) override;
		virtual DX12Engine::GameObject* GetInteractiveObject() override;

	private:
		// The anchor is kept alongside the light so Update can position it absolutely each
		// frame; integrating an offset instead lets float error walk the field out of the hall.
		struct AnimatedLight
		{
			std::shared_ptr<DX12Engine::Light> Source;
			DirectX::XMFLOAT3 Anchor;
			float Phase;
		};

		void BuildLights();
		void BuildGeometry();
		std::shared_ptr<DX12Engine::GameObject> SpawnObject(const std::string& name, std::shared_ptr<DX12Engine::ModelAsset> model, DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 scale);

		std::shared_ptr<DX12Engine::RenderContext> m_RenderContext;
		std::shared_ptr<DX12Engine::PhysicsEngine> m_PhysicsEngine;
		DirectX::XMFLOAT2 m_WindowSize;

		std::vector<AnimatedLight> m_GridLights;
		std::vector<AnimatedLight> m_OverlapLights;
		std::vector<std::shared_ptr<DX12Engine::Light>> m_SweepSpots;
		std::shared_ptr<DX12Engine::GameObject> m_InteractiveObject;

		float m_Time = 0.0f;
	};
}
