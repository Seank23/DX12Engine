#include "PhysicsComponent.h"
#include "GameObject.h"

namespace DX12Engine
{
    PhysicsComponent::PhysicsComponent(GameObject* parent)
		: Component(parent, ComponentType::Physics),
		m_Velocity(0.0f, 0.0f, 0.0f),
		m_Acceleration(0.0f, 0.0f, 0.0f),
		m_Mass(1.0f),
		m_IsStatic(false)
    {
    }

    PhysicsComponent::~PhysicsComponent()
    {
    }

    void PhysicsComponent::Init()
    {
    }

    void PhysicsComponent::Update(float ts, float elapsed)
    {
		if (!m_IsStatic)
		{
			for (Force& force : m_Forces)
			{
				if (force.Duration > 0.0f)
				{
					m_Acceleration = DirectX::XMVectorAdd(m_Acceleration, DirectX::XMVectorScale(force.Magnitude, 1.0f / m_Mass));
					force.Duration -= ts;
				}
			}
			m_Forces.erase(std::remove_if(m_Forces.begin(), m_Forces.end(), [](Force f) { return f.Duration <= 0.0f; }), m_Forces.end());
			m_Velocity = DirectX::XMVectorAdd(m_Velocity, DirectX::XMVectorScale(m_Acceleration, ts));
			m_Parent->Move(DirectX::XMVectorScale(m_Velocity, ts));
			m_Acceleration = DirectX::XMVectorZero();
		}
    }

	void PhysicsComponent::ApplyForce(Force force)
	{
		if (!m_IsStatic)
			m_Forces.push_back(force);
	}
}
