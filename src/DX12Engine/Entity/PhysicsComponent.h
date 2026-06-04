#pragma once
#include "Component.h"
#include <vector>
#include <DirectXMath.h>
#include "../Physics/AABoundingBox.h"
#include "../Physics/CollisionMesh.h"

constexpr auto APPLY_GRAVITY = 1;
constexpr auto GRAVITY = 9.81f;

namespace DX12Engine
{
	struct Force
	{
		DirectX::XMVECTOR Magnitude;
		float Duration = 0.05f;
		DirectX::XMVECTOR Point = DirectX::XMVectorZero();
		bool IsLocalSpace = false;
		bool HasPoint = false;
	};

	class PhysicsComponent : public Component, public IColliderListener
	{
	public:
		friend class PhysicsEngine;

		PhysicsComponent(GameObject* parent);
		~PhysicsComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;
		void IntegrateVelocity(float ts);
		void IntegratePosition(float ts);

		virtual void OnColliderChanged(ColliderComponent* colliderComponent) override;
		virtual void OnTransformChanged(TransformType type) override;

		void ApplyForce(Force force);
		void ApplyTorque(DirectX::XMVECTOR torque);

		void SetMass(float mass);
		void SetIsStatic(bool isStatic);
		void SetRestitution(float restitution) { m_Restitution = restitution; }
		void SetStaticFriction(float friction) { m_StaticFriction = friction; }
		void SetKineticFriction(float friction) { m_KineticFriction = friction; }
		void SetLinearDamping(float damping) { m_LinearDamping = damping; }
		void SetAngularDamping(float damping) { m_AngularDamping = damping; }

		const AABoundingBox& GetBoundingBox() const;
		const CollisionMesh& GetCollisionMesh() const;
		DirectX::XMVECTOR GetPosition();

	private:
		void EvaluateForces(float ts);
		void RecomputeLocalInertiaTensor();
		void UpdateInertiaTensor();
		DirectX::XMFLOAT3 GetCollisionDimensionsForInertia() const;
		bool ShouldRest(float ts);

		DirectX::XMVECTOR m_Velocity;
		DirectX::XMVECTOR m_AngularVelocity;
		DirectX::XMVECTOR m_PseudoVelocity;
		DirectX::XMVECTOR m_Acceleration;
		DirectX::XMVECTOR m_Torque;
		DirectX::XMMATRIX m_InverseInertiaTensor;
		DirectX::XMMATRIX m_LocalInertiaTensor;

		float m_Mass;
		float m_InvMass;
		bool m_IsStatic;
		float m_LinearDamping = 0.02f;
		float m_AngularDamping = 0.05f;
		float m_Restitution = 0.5f;
		float m_StaticFriction = 0.5f;
		float m_KineticFriction = 0.3f;
		float m_TimeBelowSleepThreshold = 0.0f;
		bool m_ManagedByEngine = false;

		std::vector<Force> m_Forces;

		ColliderComponent* m_ColliderComponent = nullptr;
	};
}
