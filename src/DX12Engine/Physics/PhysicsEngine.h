#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "../Entity/PhysicsComponent.h"

constexpr auto SIMULATION_RATE = 1.0f;

namespace DX12Engine
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine() = default;
		~PhysicsEngine() = default;

		void Update(float ts, float elapsed);

		void SetComponents(std::vector<PhysicsComponent*> components) { m_Components = components; }

	private:
		bool CheckCollision(PhysicsComponent* a, PhysicsComponent* b, ContactManifold* outContact);
		void PositionalCorrection(ContactManifold& contact);
		void ResolveCollision(ContactManifold& contact, float ts);
		DirectX::XMVECTOR CalculateContactTangent(DirectX::XMVECTOR normal);

		std::vector<PhysicsComponent*> m_Components;
	};
}

