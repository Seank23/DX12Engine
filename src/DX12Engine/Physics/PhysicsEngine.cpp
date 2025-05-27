#include "PhysicsEngine.h"

namespace DX12Engine
{
	void PhysicsEngine::Update(float ts, float elapsed)
	{
		for (const auto& component : m_Components)
		{
			if (!component->m_IsStatic)
			{
				if (APPLY_GRAVITY)
					component->m_Acceleration = DirectX::XMVECTOR({ 0.0f, -GRAVITY, 0.0f });
			}
		}
	}
}
