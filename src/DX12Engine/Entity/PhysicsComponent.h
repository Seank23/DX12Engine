#pragma once
#include "Component.h"
#include <vector>
#include <DirectXMath.h>
#include "../Physics/AABoundingBox.h"
#include "../Physics/CollisionMesh.h"

namespace DX12Engine
{
	struct Force
	{
		DirectX::XMVECTOR Magnitude;
		float Duration = 0.05f;
		DirectX::XMVECTOR Point;
	};

	class PhysicsComponent : public Component
	{
	public:
		friend class PhysicsEngine;

		PhysicsComponent(GameObject* parent);
		~PhysicsComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;

		virtual void OnMeshChanged(Mesh* newMesh) override;
		virtual void OnTransformChanged(TransformType type) override;

		void ApplyForce(Force force);

		void SetMass(float mass);
		void SetIsStatic(bool isStatic);

		void SetCollisionMeshType(CollisionMeshType type);

		AABoundingBox& GetBoundingBox() { return m_BoundingBox; }
		CollisionMesh& GetCollisionMesh() { return m_CollisionMesh; }
		DirectX::XMVECTOR GetPosition();

	private:
		void EvaluateForces(float ts);
		void UpdateInertiaTensor();
		std::vector<DirectX::XMVECTOR> GetBoundingBoxVertices(std::vector<DirectX::XMVECTOR> transformedVertices);
		void UpdateCollisionMesh();

		DirectX::XMVECTOR m_Velocity;
		DirectX::XMVECTOR m_AngularVelocity;
		DirectX::XMVECTOR m_Acceleration;
		DirectX::XMVECTOR m_Torque;
		DirectX::XMMATRIX m_InverseInertiaTensor;
		DirectX::XMMATRIX m_LocalInertiaTensor;

		float m_Mass;
		float m_InvMass;
		bool m_IsStatic;

		std::vector<Force> m_Forces;

		AABoundingBox m_BoundingBox;
		CollisionMesh m_CollisionMesh;
	};
}

