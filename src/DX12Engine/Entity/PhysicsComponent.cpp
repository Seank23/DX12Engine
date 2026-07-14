#define NOMINMAX
#include "PhysicsComponent.h"
#include "ColliderComponent.h"
#include "GameObject.h"

#include <algorithm>
#include <cmath>

namespace DX12Engine
{
	PhysicsComponent::PhysicsComponent(GameObject* parent)
		: Component(parent, ComponentType::Physics, true),
		  m_Velocity(DirectX::XMVectorZero()),
		  m_AngularVelocity(DirectX::XMVectorZero()),
		  m_PseudoVelocity(DirectX::XMVectorZero()),
		  m_Acceleration(DirectX::XMVectorZero()),
		  m_Torque(DirectX::XMVectorZero()),
		  m_InverseInertiaTensor(DirectX::XMMatrixIdentity()),
		  m_LocalInertiaTensor(DirectX::XMMatrixIdentity()),
		  m_Mass(1.0f),
		  m_InvMass(1.0f),
		  m_IsStatic(false)
	{
		if (ColliderComponent* collider = parent->GetComponent<ColliderComponent>())
			OnColliderChanged(collider);
	}

	PhysicsComponent::~PhysicsComponent()
	{
	}

	void PhysicsComponent::Init()
	{
	}

	void PhysicsComponent::Update(float ts, float elapsed)
	{
		if (m_ManagedByEngine)
			return;

		IntegrateVelocity(ts);
		IntegratePosition(ts);
	}

	void PhysicsComponent::IntegrateVelocity(float ts)
	{
		if (m_IsStatic)
		{
			m_Acceleration = DirectX::XMVectorZero();
			return;
		}

		if (ShouldRest(ts))
		{
			m_Acceleration = DirectX::XMVectorZero();
			return;
		}

		if (APPLY_GRAVITY)
		{
			m_Acceleration = DirectX::XMVectorAdd(
				m_Acceleration,
				DirectX::XMVectorSet(0.0f, -GRAVITY, 0.0f, 0.0f));
		}

		EvaluateForces(ts);
		m_Velocity = DirectX::XMVectorAdd(m_Velocity, DirectX::XMVectorScale(m_Acceleration, ts));
		m_Velocity = DirectX::XMVectorScale(m_Velocity, std::powf(1.0f - m_LinearDamping, ts));

		if (!DirectX::XMVector4Equal(m_Torque, DirectX::XMVectorZero()) || !DirectX::XMVector4Equal(m_AngularVelocity, DirectX::XMVectorZero()))
		{
			DirectX::XMVECTOR angularAcceleration = DirectX::XMVector3Transform(m_Torque, m_InverseInertiaTensor);
			m_AngularVelocity = DirectX::XMVectorAdd(m_AngularVelocity, DirectX::XMVectorScale(angularAcceleration, ts));
			m_AngularVelocity = DirectX::XMVectorScale(m_AngularVelocity, std::powf(1.0f - m_AngularDamping, ts));
			UpdateInertiaTensor();
		}

		m_Acceleration = DirectX::XMVectorZero();
		m_Torque = DirectX::XMVectorZero();
	}

	void PhysicsComponent::IntegratePosition(float ts)
	{
		if (m_IsStatic)
			return;

		if (!DirectX::XMVector4Equal(m_AngularVelocity, DirectX::XMVectorZero()))
		{
			DirectX::XMVECTOR rotation = m_Parent->GetRotation();
			DirectX::XMVECTOR omegaQuat = DirectX::XMVectorSet(
				DirectX::XMVectorGetX(m_AngularVelocity),
				DirectX::XMVectorGetY(m_AngularVelocity),
				DirectX::XMVectorGetZ(m_AngularVelocity),
				0.0f);
			DirectX::XMVECTOR delta = DirectX::XMVectorScale(DirectX::XMQuaternionMultiply(omegaQuat, rotation), 0.5f);
			rotation = DirectX::XMVector4Normalize(DirectX::XMVectorAdd(rotation, DirectX::XMVectorScale(delta, ts)));
			m_Parent->SetRotationQuaternion(rotation);
		}
		m_Parent->Move(DirectX::XMVectorScale(m_Velocity, ts));
	}

	void PhysicsComponent::OnColliderChanged(ColliderComponent* colliderComponent)
	{
		m_ColliderComponent = colliderComponent;
		if (m_IsStatic || m_Mass <= 0.0f)
			return;

		RecomputeLocalInertiaTensor();
		UpdateInertiaTensor();
	}

	void PhysicsComponent::OnTransformChanged(TransformType type)
	{
		if (m_IsStatic)
			return;

		if (type == TransformType::Scale)
		{
			RecomputeLocalInertiaTensor();
			UpdateInertiaTensor();
			return;
		}

		if (type == TransformType::Rotation)
			UpdateInertiaTensor();
	}

	void PhysicsComponent::ApplyForce(Force force)
	{
		if (!m_IsStatic)
			m_Forces.push_back(force);
	}

	void PhysicsComponent::ApplyTorque(DirectX::XMVECTOR torque)
	{
		if (!m_IsStatic)
			m_Torque = DirectX::XMVectorAdd(m_Torque, torque);
	}

	void PhysicsComponent::SetMass(float mass)
	{
		m_Mass = mass;
		if (m_Mass <= 0.0f)
		{
			SetIsStatic(true);
			return;
		}

		m_IsStatic = false;
		m_InvMass = 1.0f / m_Mass;
		RecomputeLocalInertiaTensor();
		UpdateInertiaTensor();
	}

	void PhysicsComponent::SetIsStatic(bool isStatic)
	{
		m_IsStatic = isStatic;
		if (isStatic)
		{
			m_InverseInertiaTensor = DirectX::XMMatrixSet(
				0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
			m_InvMass = 0.0f;
			m_Velocity = DirectX::XMVectorZero();
			m_AngularVelocity = DirectX::XMVectorZero();
			m_Acceleration = DirectX::XMVectorZero();
			return;
		}

		if (m_Mass <= 0.0f)
			m_Mass = 1.0f;

		m_InvMass = 1.0f / m_Mass;
		RecomputeLocalInertiaTensor();
		UpdateInertiaTensor();
	}

	const AABoundingBox& PhysicsComponent::GetBoundingBox() const
	{
		if (m_ColliderComponent)
			return m_ColliderComponent->GetBoundingBox();

		static const AABoundingBox kEmptyBounds{};
		return kEmptyBounds;
	}

	const CollisionMesh& PhysicsComponent::GetCollisionMesh() const
	{
		if (m_ColliderComponent)
			return m_ColliderComponent->GetCollisionMesh();

		static const CollisionMesh kEmptyMesh{};
		return kEmptyMesh;
	}

	DirectX::XMVECTOR PhysicsComponent::GetPosition()
	{
		return m_Parent->GetPosition();
	}

	void PhysicsComponent::EvaluateForces(float ts)
	{
		for (Force& force : m_Forces)
		{
			if (force.Duration >= 0.0f)
			{
				m_Acceleration = DirectX::XMVectorAdd(m_Acceleration, DirectX::XMVectorScale(force.Magnitude, 1.0f / m_Mass));
				force.Duration -= ts;
				if (force.HasPoint)
				{
					DirectX::XMVECTOR worldPoint = force.Point;
					if (force.IsLocalSpace)
						worldPoint = DirectX::XMVectorAdd(m_Parent->GetPosition(),
														  DirectX::XMVector3Rotate(force.Point, m_Parent->GetRotation()));
					DirectX::XMVECTOR leverArm = DirectX::XMVectorSubtract(worldPoint, m_Parent->GetPosition());
					DirectX::XMVECTOR torque = DirectX::XMVector3Cross(leverArm, force.Magnitude);
					m_Torque = DirectX::XMVectorAdd(m_Torque, torque);
				}
			}
		}
		m_Forces.erase(std::remove_if(m_Forces.begin(), m_Forces.end(), [](const Force& f)
									  { return f.Duration <= 0.0f; }),
					   m_Forces.end());
	}

	void PhysicsComponent::RecomputeLocalInertiaTensor()
	{
		if (m_IsStatic || m_Mass <= 0.0f)
			return;

		const DirectX::XMFLOAT3 dims = GetCollisionDimensionsForInertia();
		const float w = (std::max)(std::fabs(dims.x), 0.001f);
		const float h = (std::max)(std::fabs(dims.y), 0.001f);
		const float d = (std::max)(std::fabs(dims.z), 0.001f);
		const float k = m_Mass / 12.0f;

		m_LocalInertiaTensor = DirectX::XMMatrixSet(
			k * (h * h + d * d), 0.0f, 0.0f, 0.0f, 0.0f, k * (w * w + d * d), 0.0f, 0.0f, 0.0f, 0.0f, k * (w * w + h * h), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	}

	void PhysicsComponent::UpdateInertiaTensor()
	{
		if (m_IsStatic || m_InvMass <= 0.0f)
			return;

		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation());
		DirectX::XMMATRIX worldInertiaTensor = DirectX::XMMatrixMultiply(rotationMatrix, DirectX::XMMatrixMultiply(m_LocalInertiaTensor, DirectX::XMMatrixTranspose(rotationMatrix)));
		m_InverseInertiaTensor = DirectX::XMMatrixInverse(nullptr, worldInertiaTensor);
	}

	DirectX::XMFLOAT3 PhysicsComponent::GetCollisionDimensionsForInertia() const
	{
		const CollisionMesh& collisionMesh = GetCollisionMesh();
		switch (collisionMesh.Type)
		{
		case CollisionMeshType::Box:
			return {
				2.0f * DirectX::XMVectorGetX(collisionMesh.OBBData.Extents),
				2.0f * DirectX::XMVectorGetY(collisionMesh.OBBData.Extents),
				2.0f * DirectX::XMVectorGetZ(collisionMesh.OBBData.Extents)
			};
		case CollisionMeshType::Sphere:
		{
			const float diameter = 2.0f * collisionMesh.SphereData.Radius;
			return { diameter, diameter, diameter };
		}
		case CollisionMeshType::Plane:
			return {
				2.0f * collisionMesh.PlaneData.HalfExtent0,
				0.02f,
				2.0f * collisionMesh.PlaneData.HalfExtent1
			};
		case CollisionMeshType::None:
		default:
		{
			const AABoundingBox& bounds = GetBoundingBox();
			if (!bounds.Vertices.empty())
				return bounds.Dimensions;
			return { 1.0f, 1.0f, 1.0f };
		}
		}
	}

	bool PhysicsComponent::ShouldRest(float ts)
	{
		if (m_Forces.size() > 0 || !DirectX::XMVector3Equal(m_Torque, DirectX::XMVectorZero()))
		{
			m_TimeBelowSleepThreshold = 0.0f;
			return false;
		}

		if (GetCollisionMesh().Type == CollisionMeshType::Box)
		{
			const DirectX::XMVECTOR worldUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			const DirectX::XMVECTOR q = m_Parent->GetRotation();
			const DirectX::XMVECTOR localAxes[3] = {
				DirectX::XMVector3Rotate(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), q),
				DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), q),
				DirectX::XMVector3Rotate(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), q)
			};
			constexpr float kFaceAlignThreshold = 0.966f;
			bool stable = false;
			for (int i = 0; i < 3; ++i)
			{
				float d = std::abs(DirectX::XMVectorGetX(DirectX::XMVector3Dot(localAxes[i], worldUp)));
				if (d >= kFaceAlignThreshold)
				{
					stable = true;
					break;
				}
			}
			if (!stable)
			{
				m_TimeBelowSleepThreshold = 0.0f;
				return false;
			}
		}

		const float sleepLinearThreshold = 0.05f;
		const float sleepAngularThreshold = 0.1f;
		const float sleepTimeThreshold = 0.5f;

		float linearSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(m_Velocity));
		float angularSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(m_AngularVelocity));

		if (linearSpeed < sleepLinearThreshold && angularSpeed < sleepAngularThreshold)
		{
			m_TimeBelowSleepThreshold += ts;
			if (m_TimeBelowSleepThreshold >= sleepTimeThreshold)
			{
				m_Velocity = DirectX::XMVectorZero();
				m_AngularVelocity = DirectX::XMVectorZero();
				m_TimeBelowSleepThreshold = 0.0f;
				return true;
			}
		}
		else
		{
			m_TimeBelowSleepThreshold = 0.0f;
		}
		return false;
	}
}
