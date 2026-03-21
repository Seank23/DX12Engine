#define NOMINMAX
#include "PhysicsEngine.h"
#include <cmath>
#include "../Entity/GameObject.h"
#include <algorithm>

namespace DX12Engine
{

	void PhysicsEngine::Update(float ts, float elapsed)
	{
		m_Accumulator += ts;
		int steps = 0;
		while (m_Accumulator >= FIXED_DT && steps < MAX_SUBSTEPS)
		{
			Step(FIXED_DT * SIMULATION_RATE, elapsed);
			m_Accumulator -= FIXED_DT;
			++steps;
		}
		if (m_Accumulator > FIXED_DT * MAX_SUBSTEPS)
			m_Accumulator = 0.0f;
	}

	void PhysicsEngine::SetComponents(std::vector<PhysicsComponent*> components)
	{
		m_Components = components;
		for (auto* c : m_Components)
			c->m_ManagedByEngine = true;
	}

	void PhysicsEngine::Step(float dt, float elapsed)
	{
		// Semi-implicit Euler: integrate forces into velocity first
		for (const auto& component : m_Components)
			component->IntegrateVelocity(dt);

		// Detect collisions using current positions
		std::vector<ContactManifold> contacts;
		for (int i = 0; i < m_Components.size(); i++)
		{
			for (int j = i + 1; j < m_Components.size(); j++)
			{
				ContactManifold contact;
				if (CheckCollision(m_Components[i], m_Components[j], &contact))
				{
					contacts.push_back(contact);
				}
			}
		}

		for (auto& contact : contacts)
		{
			if (contact.Contacts.size() > 0)
			{
				ResolveCollision(contact, dt);
				PositionalCorrection(contact);
			}
		}

		for (const auto& component : m_Components)
			component->IntegratePosition(dt);

		for (const auto& component : m_Components)
			component->Update(dt, elapsed);
	}

	bool PhysicsEngine::CheckCollision(PhysicsComponent* a, PhysicsComponent* b, ContactManifold* outContact)
	{
		if (a->GetBoundingBox().Intersects(b->GetBoundingBox()))
		{
			if (a->GetCollisionMesh().Intersects(b->GetCollisionMesh(), outContact))
			{
				outContact->A = b->m_InvMass == 0.0f ? b : a;
				outContact->B = b->m_InvMass == 0.0f ? a : b;
				return true;
			}
		}
		return false;
	}

	void PhysicsEngine::PositionalCorrection(ContactManifold& contact)
	{
		const float percent = 0.3f;
		const float slop = 0.005f;

		float totalInverseMass = contact.A->m_InvMass + contact.B->m_InvMass;
		if (totalInverseMass == 0.0f)
			return;

		float penetration = contact.PenetrationDepth - slop;
		if (penetration > 0.0f)
		{
			DirectX::XMVECTOR correction = DirectX::XMVectorScale(contact.Normal, (penetration / totalInverseMass) * percent);

			contact.A->m_Parent->Move(DirectX::XMVectorNegate(DirectX::XMVectorScale(correction, contact.A->m_InvMass)));
			contact.B->m_Parent->Move(DirectX::XMVectorScale(correction, contact.B->m_InvMass));
		}
	}

	void PhysicsEngine::ResolveCollision(ContactManifold& contact, float ts)
	{
		PhysicsComponent* a = contact.A;
		PhysicsComponent* b = contact.B;

		float restitution     = std::min(a->m_Restitution,    b->m_Restitution);
		float staticFriction  = std::sqrt(a->m_StaticFriction  * b->m_StaticFriction);
		float kineticFriction = std::sqrt(a->m_KineticFriction * b->m_KineticFriction);

		constexpr float kRestitutionSlop = 0.5f;

		const int numContacts = static_cast<int>(contact.Contacts.size());

		// Average contact point used for both passes.
		DirectX::XMVECTOR avgContact = DirectX::XMVectorZero();
		for (int ci = 0; ci < numContacts; ++ci)
			avgContact = DirectX::XMVectorAdd(avgContact, contact.Contacts[ci].Point);
		avgContact = DirectX::XMVectorScale(avgContact, 1.0f / numContacts);

		DirectX::XMVECTOR ra = DirectX::XMVectorSubtract(avgContact, a->GetPosition());
		DirectX::XMVECTOR rb = DirectX::XMVectorSubtract(avgContact, b->GetPosition());

		// --- Pass 1: normal impulse ---
		// Velocity at the contact point includes ω×r so the constraint correctly
		// models a body pivoting about a contact edge under gravity.
		DirectX::XMVECTOR contactVelA = DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, ra));
		DirectX::XMVECTOR contactVelB = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, rb));
		float velocityAlongNormal = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
			DirectX::XMVectorSubtract(contactVelB, contactVelA), contact.Normal));

		float totalJn = 0.0f;

		if (velocityAlongNormal < 0.0f)
		{
			float e = (std::abs(velocityAlongNormal) > kRestitutionSlop) ? restitution : 0.0f;

			DirectX::XMVECTOR raCrossN = DirectX::XMVector3Cross(ra, contact.Normal);
			DirectX::XMVECTOR rbCrossN = DirectX::XMVector3Cross(rb, contact.Normal);
			float angularTermA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
				DirectX::XMVector3Cross(DirectX::XMVector3Transform(raCrossN, a->m_InverseInertiaTensor), ra), contact.Normal));
			float angularTermB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
				DirectX::XMVector3Cross(DirectX::XMVector3Transform(rbCrossN, b->m_InverseInertiaTensor), rb), contact.Normal));
			float effectiveMass = a->m_InvMass + b->m_InvMass + angularTermA + angularTermB;
			if (effectiveMass <= 0.0f) return;

			float jn = -(1.0f + e) * velocityAlongNormal / effectiveMass;
			if (jn <= 0.0f) return;

			totalJn = jn;
			DirectX::XMVECTOR normalImpulse = DirectX::XMVectorScale(contact.Normal, jn);

			if (a->m_InvMass > 0.0f)
			{
				a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(normalImpulse, a->m_InvMass));
				a->m_AngularVelocity = DirectX::XMVectorSubtract(a->m_AngularVelocity,
					DirectX::XMVector3Transform(DirectX::XMVectorScale(raCrossN, jn), a->m_InverseInertiaTensor));
			}
			if (b->m_InvMass > 0.0f)
			{
				b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(normalImpulse, b->m_InvMass));
				b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity,
					DirectX::XMVector3Transform(DirectX::XMVectorScale(rbCrossN, jn), b->m_InverseInertiaTensor));
			}
		}

		// --- Pass 2: friction impulse ---
		// Relative velocity at the contact point includes ω×r so friction correctly
		// sees rotational motion (enabling tipping onto a face) and can damp spin.
		DirectX::XMVECTOR relVel = DirectX::XMVectorSubtract(
			DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, rb)),
			DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, ra)));

		DirectX::XMVECTOR tangent = DirectX::XMVectorSubtract(relVel,
			DirectX::XMVectorScale(contact.Normal,
				DirectX::XMVectorGetX(DirectX::XMVector3Dot(relVel, contact.Normal))));

		float tangentLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(tangent));
		if (tangentLength < 1e-4f)
			return;

		tangent = DirectX::XMVectorScale(tangent, 1.0f / tangentLength);

		DirectX::XMVECTOR raCrossT = DirectX::XMVector3Cross(ra, tangent);
		DirectX::XMVECTOR rbCrossT = DirectX::XMVector3Cross(rb, tangent);
		float angularTermTA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
			DirectX::XMVector3Cross(DirectX::XMVector3Transform(raCrossT, a->m_InverseInertiaTensor), ra), tangent));
		float angularTermTB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
			DirectX::XMVector3Cross(DirectX::XMVector3Transform(rbCrossT, b->m_InverseInertiaTensor), rb), tangent));
		float effectiveMassT = a->m_InvMass + b->m_InvMass + angularTermTA + angularTermTB;
		if (effectiveMassT <= 0.0f) return;

		float jt = -tangentLength / effectiveMassT;

		DirectX::XMVECTOR frictionImpulse = (std::abs(jt) <= staticFriction * totalJn)
			? DirectX::XMVectorScale(tangent, jt)
			: DirectX::XMVectorScale(tangent, -kineticFriction * totalJn);

		if (a->m_InvMass > 0.0f)
		{
			// Clamp the linear friction impulse so it does not reverse the
			// tangential component of linear velocity. Without this, residual
			// angular velocity (ω×r) at the contact is misread as sliding and
			// friction converts rotational energy into lateral linear motion.
			float tanVelA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(a->m_Velocity, tangent));
			float linearDeltaA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(frictionImpulse, tangent)) * a->m_InvMass;
			float clampedLinearDeltaA = linearDeltaA;
			if (std::abs(tanVelA) > 1e-6f && (tanVelA - linearDeltaA) * tanVelA < 0.0f)
				clampedLinearDeltaA = tanVelA;
			a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity,
				DirectX::XMVectorScale(tangent, clampedLinearDeltaA));
			a->m_AngularVelocity = DirectX::XMVectorAdd(a->m_AngularVelocity,
				DirectX::XMVector3Transform(
					DirectX::XMVector3Cross(ra, DirectX::XMVectorNegate(frictionImpulse)),
					a->m_InverseInertiaTensor));
		}
		if (b->m_InvMass > 0.0f)
		{
			float tanVelB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(b->m_Velocity, tangent));
			float linearDeltaB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(frictionImpulse, tangent)) * b->m_InvMass;
			float clampedLinearDeltaB = linearDeltaB;
			if (std::abs(tanVelB) > 1e-6f && (tanVelB + linearDeltaB) * tanVelB < 0.0f)
				clampedLinearDeltaB = -tanVelB;
			b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity,
				DirectX::XMVectorScale(tangent, clampedLinearDeltaB));
			b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity,
				DirectX::XMVector3Transform(
					DirectX::XMVector3Cross(rb, frictionImpulse),
					b->m_InverseInertiaTensor));
		}
	}
}
