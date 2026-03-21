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

	class PhysicsComponent : public Component
	{
	public:
		friend class PhysicsEngine;

		PhysicsComponent(GameObject* parent);
		~PhysicsComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;
		void IntegrateVelocity(float ts);
		void IntegratePosition(float ts);

		virtual void OnMeshChanged(Mesh* newMesh) override;
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

		void SetCollisionMeshType(CollisionMeshType type);

		AABoundingBox& GetBoundingBox() { return m_BoundingBox; }
		CollisionMesh& GetCollisionMesh() { return m_CollisionMesh; }
		DirectX::XMVECTOR GetPosition();

	private:
		void EvaluateForces(float ts);
		void UpdateInertiaTensor();
		std::vector<DirectX::XMVECTOR> GetBoundingBoxVertices(std::vector<DirectX::XMVECTOR> transformedVertices);
		void UpdateCollisionMesh();
		bool ShouldRest(float ts);

		DirectX::XMFLOAT3 m_LocalHalfExtents = { 0.0f, 0.0f, 0.0f };

		DirectX::XMVECTOR m_Velocity;
		DirectX::XMVECTOR m_AngularVelocity;
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

		AABoundingBox m_BoundingBox;
		CollisionMesh m_CollisionMesh;
	};
}
