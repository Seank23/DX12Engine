#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include "../Entity/PhysicsComponent.h"

constexpr auto APPLY_GRAVITY = 1;
constexpr auto GRAVITY = 9.81f;

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
		std::vector<PhysicsComponent*> m_Components;
	};
}

