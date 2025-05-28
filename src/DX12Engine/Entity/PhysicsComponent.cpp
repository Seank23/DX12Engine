#include "PhysicsComponent.h"
#include "GameObject.h"

namespace DX12Engine
{
    PhysicsComponent::PhysicsComponent(GameObject* parent)
		: Component(parent, ComponentType::Physics, true),
		m_Velocity(DirectX::XMVectorZero()),
		m_AngularVelocity(DirectX::XMVectorZero()),
		m_Acceleration(DirectX::XMVectorZero()),
		m_Torque(DirectX::XMVectorZero()),
		m_InverseInertiaTensor(DirectX::XMMatrixIdentity()),
		m_LocalInertiaTensor(DirectX::XMMatrixIdentity()),
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
			EvaluateForces(ts);
			m_Velocity = DirectX::XMVectorAdd(m_Velocity, DirectX::XMVectorScale(m_Acceleration, ts));

			if (!DirectX::XMVector4Equal(m_Torque, DirectX::XMVectorZero()) || !DirectX::XMVector4Equal(m_AngularVelocity, DirectX::XMVectorZero()))
			{
				DirectX::XMVECTOR angularAccceleration = DirectX::XMVector3Transform(m_Torque, m_InverseInertiaTensor);
				m_AngularVelocity = DirectX::XMVectorAdd(m_AngularVelocity, DirectX::XMVectorScale(angularAccceleration, ts));
				UpdateInertiaTensor();

				DirectX::XMVECTOR rotation = m_Parent->GetRotation();
				DirectX::XMVECTOR delta = DirectX::XMQuaternionMultiply(rotation, DirectX::XMVectorScale(m_AngularVelocity, 0.5f));
				rotation = DirectX::XMVector4Normalize(DirectX::XMVectorAdd(rotation, DirectX::XMVectorScale(delta, ts)));
				m_Parent->SetRotationQuaternion(rotation);
			}

			m_Parent->Move(DirectX::XMVectorScale(m_Velocity, ts));

			m_Acceleration = DirectX::XMVectorZero();
			m_Torque = DirectX::XMVectorZero();
		}
    }

	void PhysicsComponent::ApplyForce(Force force)
	{
		if (!m_IsStatic)
			m_Forces.push_back(force);
	}

	void PhysicsComponent::EvaluateForces(float ts)
	{
		for (Force& force : m_Forces)
		{
			if (force.Duration > 0.0f)
			{
				m_Acceleration = DirectX::XMVectorAdd(m_Acceleration, DirectX::XMVectorScale(force.Magnitude, 1.0f / m_Mass));
				force.Duration -= ts;
				if (!DirectX::XMVector3Equal(force.Point, DirectX::XMVectorZero()))
				{
					DirectX::XMVECTOR position = m_Parent->GetPosition();
					DirectX::XMVECTOR torque = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(force.Point, position), force.Magnitude);
					m_Torque = DirectX::XMVectorAdd(m_Torque, torque);
				}
			}
		}
		m_Forces.erase(std::remove_if(m_Forces.begin(), m_Forces.end(), [](Force f) { return f.Duration <= 0.0f; }), m_Forces.end());
	}

	void PhysicsComponent::UpdateInertiaTensor()
	{
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation());
		DirectX::XMMATRIX worldInertiaTensor = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranspose(rotationMatrix), DirectX::XMMatrixMultiply(m_LocalInertiaTensor, rotationMatrix));
		m_InverseInertiaTensor = DirectX::XMMatrixInverse(nullptr, worldInertiaTensor);
	}
}
