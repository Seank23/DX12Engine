#pragma once
#include "Component.h"
#include <vector>
#include <DirectXMath.h>

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

		void ApplyForce(Force force);

		void SetMass(float mass) { m_Mass = mass; }
		void SetIsStatic(bool isStatic) { m_IsStatic = isStatic; }

	private:
		void EvaluateForces(float ts);
		void UpdateInertiaTensor();

		DirectX::XMVECTOR m_Velocity;
		DirectX::XMVECTOR m_AngularVelocity;
		DirectX::XMVECTOR m_Acceleration;
		DirectX::XMVECTOR m_Torque;
		DirectX::XMMATRIX m_InverseInertiaTensor;
		DirectX::XMMATRIX m_LocalInertiaTensor;

		float m_Mass;
		bool m_IsStatic;

		std::vector<Force> m_Forces;
	};
}

