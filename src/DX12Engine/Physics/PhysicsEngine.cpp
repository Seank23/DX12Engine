#include "PhysicsEngine.h"
#include <cmath>
#include "../Entity/GameObject.h"
#include <algorithm>

namespace DX12Engine
{
	namespace
	{
		constexpr float kCollisionForceDuration = 0.01f;
	}

	void PhysicsEngine::Update(float ts, float elapsed)
	{
		ts *= SIMULATION_RATE;
		for (const auto& component : m_Components)
		{
			if (!component->m_IsStatic && APPLY_GRAVITY)
			{
				component->m_Acceleration = DirectX::XMVectorAdd(
					component->m_Acceleration,
					DirectX::XMVectorSet(0.0f, -GRAVITY, 0.0f, 0.0f)
				);
			}
		}

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
				PositionalCorrection(contact);
				ResolveCollision(contact);
			}
		}

		for (const auto& component : m_Components)
			component->Update(ts, elapsed);
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
		const float percent = 0.02f;
		const float slop = 0.01f;

		float penetration = contact.PenetrationDepth - slop;
		if (penetration > 0.0f) {
			DirectX::XMVECTOR correctionDir = contact.Normal;
			float totalInverseMass = contact.A->m_InvMass + contact.B->m_InvMass;
			DirectX::XMVECTOR correction = DirectX::XMVectorScale(correctionDir, (penetration / totalInverseMass) * percent);

			contact.A->m_Parent->Move(DirectX::XMVectorNegate(DirectX::XMVectorMultiply(correction, DirectX::XMVectorReplicate(contact.A->m_InvMass))));
			contact.B->m_Parent->Move(DirectX::XMVectorMultiply(correction, DirectX::XMVectorReplicate(contact.B->m_InvMass)));
		}
	}

	void PhysicsEngine::ResolveCollision(ContactManifold& contact)
	{
		PhysicsComponent* a = contact.A;
		PhysicsComponent* b = contact.B;

		DirectX::XMVECTOR contactPoint = contact.Contacts[0].Point;
		DirectX::XMVECTOR ra = DirectX::XMVectorSubtract(contactPoint, a->GetPosition());
		DirectX::XMVECTOR rb = DirectX::XMVectorSubtract(contactPoint, b->GetPosition());

		DirectX::XMVECTOR velA = DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, ra));
		DirectX::XMVECTOR velB = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, rb));
		DirectX::XMVECTOR relativeVelocity = DirectX::XMVectorSubtract(velB, velA);

		float velocityAlongNormal = DirectX::XMVectorGetX(DirectX::XMVector3Dot(relativeVelocity, contact.Normal));
		if (velocityAlongNormal < 0.0f)
		{
			float restitution = 50.0f;
			float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal;
			impulseMagnitude /= a->m_InvMass + b->m_InvMass;
			DirectX::XMVECTOR impulse = DirectX::XMVectorScale(contact.Normal, impulseMagnitude * kCollisionForceDuration);

			if (a->m_InvMass > 0.0f)
			{
				a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(impulse, a->m_InvMass));
				DirectX::XMVECTOR angularImpulseA = DirectX::XMVector3Cross(ra, DirectX::XMVectorNegate(impulse));
				a->m_AngularVelocity = DirectX::XMVectorAdd(a->m_AngularVelocity, DirectX::XMVector3Transform(angularImpulseA, a->m_InverseInertiaTensor));
			}
			if (b->m_InvMass > 0.0f)
			{
				b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(impulse, b->m_InvMass));
				DirectX::XMVECTOR angularImpulseB = DirectX::XMVector3Cross(rb, impulse);
				b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity, DirectX::XMVector3Transform(angularImpulseB, b->m_InverseInertiaTensor));
			}

			DirectX::XMVECTOR tangent = DirectX::XMVectorSubtract(relativeVelocity, DirectX::XMVectorScale(contact.Normal, velocityAlongNormal));
			float tangentLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(tangent));
			if (tangentLength > 1e-4f)
				tangent = DirectX::XMVector3Normalize(tangent);
			else
				tangent = DirectX::XMVectorZero();

			float jt = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(relativeVelocity, tangent));
			jt /= a->m_InvMass + b->m_InvMass;

			float frictionCoefficient = 0.3f;
			float maxFriction = frictionCoefficient * impulseMagnitude;
			jt = std::clamp(jt, -maxFriction, maxFriction);
			if (jt != 0.0f)
			{
				DirectX::XMVECTOR frictionImpulse = DirectX::XMVectorScale(tangent, jt * kCollisionForceDuration);
				if (a->m_InvMass > 0.0f)
				{
					a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(frictionImpulse, a->m_InvMass));
					DirectX::XMVECTOR angularFrictionA = DirectX::XMVector3Cross(ra, DirectX::XMVectorNegate(frictionImpulse));
					a->m_AngularVelocity = DirectX::XMVectorAdd(a->m_AngularVelocity, DirectX::XMVector3Transform(angularFrictionA, a->m_InverseInertiaTensor));
				}
				if (b->m_InvMass > 0.0f)
				{
					b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(frictionImpulse, b->m_InvMass));
					DirectX::XMVECTOR angularFrictionB = DirectX::XMVector3Cross(rb, frictionImpulse);
					b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity, DirectX::XMVector3Transform(angularFrictionB, b->m_InverseInertiaTensor));
				}
			}
		}
	}

	DirectX::XMVECTOR PhysicsEngine::CalculateContactTangent(DirectX::XMVECTOR normal)
	{
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR right = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		DirectX::XMVECTOR tangent = DirectX::XMVector3Cross(normal, up);
		if (DirectX::XMVector3LengthSq(tangent).m128_f32[0] < 1e-4f)
		{
			tangent = DirectX::XMVector3Cross(normal, right);
		}

		return DirectX::XMVector3Normalize(tangent);
	}
}
