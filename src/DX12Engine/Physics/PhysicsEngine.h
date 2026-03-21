#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../Entity/PhysicsComponent.h"

constexpr float SIMULATION_RATE = 1.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int MAX_SUBSTEPS = 8;
constexpr int SOLVER_ITERATIONS = 8;
constexpr float BAUMGARTE_FACTOR = 0.2f;
constexpr float PENETRATION_SLOP = 0.005f;

namespace DX12Engine
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine() = default;
		~PhysicsEngine() = default;

		void Update(float ts, float elapsed);

		void SetComponents(std::vector<PhysicsComponent*> components);

	private:
		void Step(float dt, float elapsed);
		bool CheckCollision(PhysicsComponent* a, PhysicsComponent* b, ContactManifold* outContact);
		void ResolveCollision(ContactManifold& contact, float dt);
		void PositionalCorrection(ContactManifold& contact);

		std::vector<PhysicsComponent*> m_Components;
		float m_Accumulator = 0.0f;
	};
}

