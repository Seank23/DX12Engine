#include "PhysicsEngine.h"
#include <iostream>
#include "../Entity/GameObject.h"
#include <algorithm>

namespace DX12Engine
{
	void PhysicsEngine::Update(float ts, float elapsed)
	{
		ts *= SIMULATION_RATE;
		//ts = 0.002f;
		for (const auto& component : m_Components)
		{
			if (!component->m_IsStatic && APPLY_GRAVITY)
				component->m_Acceleration = DirectX::XMVECTOR({ 0.0f, -GRAVITY, 0.0f });
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
		const float percent = 0.08f;
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
			impulseMagnitude /= (a->m_InvMass + b->m_InvMass);
			DirectX::XMVECTOR impulse = DirectX::XMVectorScale(contact.Normal, impulseMagnitude);
			if (a->m_InvMass > 0.0f)
				a->ApplyForce(Force{ DirectX::XMVectorNegate(impulse), 0.01f, contactPoint });
			if (b->m_InvMass > 0.0f)
				b->ApplyForce(Force{ impulse, 0.01f, contactPoint });

			// Normal Impulse
			float e = restitution;
			DirectX::XMVECTOR contactTangent = CalculateContactTangent(contact.Normal);
			DirectX::XMVECTOR raCrossN = DirectX::XMVector3Cross(ra, contactTangent);
			DirectX::XMVECTOR rbCrossN = DirectX::XMVector3Cross(rb, contactTangent);
			float angularTermA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3TransformNormal(raCrossN, a->m_InverseInertiaTensor), raCrossN));
			float angularTermB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3TransformNormal(rbCrossN, b->m_InverseInertiaTensor), rbCrossN));
			float denominator = a->m_InvMass + b->m_InvMass + angularTermA + angularTermB;
			float jn = -(1.0f + e) * velocityAlongNormal;
			jn /= denominator;
			float normalImpulseMagnitude = jn;

			// Angular Impulse
			DirectX::XMVECTOR relPositionA = DirectX::XMVectorSubtract(contactPoint, a->GetPosition());
			DirectX::XMVECTOR relPositionB = DirectX::XMVectorSubtract(contactPoint, b->GetPosition());
			DirectX::XMVECTOR relVelocityA = DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, relPositionA));
			DirectX::XMVECTOR relVelocityB = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, relPositionB));
			DirectX::XMVECTOR velocityNormalA = DirectX::XMVectorMultiply(DirectX::XMVector3Dot(relVelocityA, contact.Normal), contact.Normal);
			DirectX::XMVECTOR velocityNormalB = DirectX::XMVectorMultiply(DirectX::XMVector3Dot(relVelocityB, contact.Normal), contact.Normal);
			DirectX::XMVECTOR velocityWithoutNormalA = DirectX::XMVectorSubtract(relVelocityA, velocityNormalA);
			DirectX::XMVECTOR velocityWithoutNormalB = DirectX::XMVectorSubtract(relVelocityB, velocityNormalB);

			if (DirectX::XMVectorGetX(DirectX::XMVector3Length(velocityWithoutNormalA)) > 1e-5f)
			{
				DirectX::XMVECTOR posCrossTangent = DirectX::XMVector3Cross(relPositionA, DirectX::XMVector3Normalize(velocityWithoutNormalA));
				DirectX::XMVECTOR angularComp = DirectX::XMVector3Transform(posCrossTangent, a->m_InverseInertiaTensor);
				float angularDenom = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(angularComp, relPositionA), DirectX::XMVector3Normalize(velocityWithoutNormalA)));
				if (fabs(angularDenom) > 1e-5f)
				{
                    DirectX::XMVECTOR frictionDir = DirectX::XMVector3Normalize(velocityWithoutNormalA);
                    float frictionTorqueMag = (DirectX::XMVectorGetX(DirectX::XMVector3Length(velocityWithoutNormalA)) / angularDenom) * DirectX::XMVectorGetX(DirectX::XMVector3Length(relPositionA));
                    DirectX::XMVECTOR frictionTorque = DirectX::XMVectorScale(DirectX::XMVector3Cross(relPositionA, frictionDir), frictionTorqueMag);
                    a->ApplyTorque(DirectX::XMVectorNegate(frictionTorque));
				}
			}
			if (DirectX::XMVectorGetX(DirectX::XMVector3Length(velocityWithoutNormalB)) > 1e-5f)
			{
				DirectX::XMVECTOR posCrossTangent = DirectX::XMVector3Cross(relPositionB, DirectX::XMVector3Normalize(velocityWithoutNormalB));
				DirectX::XMVECTOR angularComp = DirectX::XMVector3Transform(posCrossTangent, b->m_InverseInertiaTensor);
				float angularDenom = DirectX::XMVectorGetX(DirectX::XMVector3Dot(DirectX::XMVector3Cross(angularComp, relPositionB), DirectX::XMVector3Normalize(velocityWithoutNormalB)));
				if (fabs(angularDenom) > 1e-5f)
				{
					DirectX::XMVECTOR frictionDir = DirectX::XMVector3Normalize(velocityWithoutNormalB);
					float frictionTorqueMag = (DirectX::XMVectorGetX(DirectX::XMVector3Length(velocityWithoutNormalB)) / angularDenom) * DirectX::XMVectorGetX(DirectX::XMVector3Length(relPositionB));
					DirectX::XMVECTOR frictionTorque = DirectX::XMVectorScale(DirectX::XMVector3Cross(relPositionB, frictionDir), frictionTorqueMag);
					b->ApplyTorque(DirectX::XMVectorNegate(frictionTorque));
				}
			}

			// Friction
			DirectX::XMVECTOR tangent = DirectX::XMVectorSubtract(relativeVelocity, DirectX::XMVectorScale(contact.Normal, velocityAlongNormal));
			float tangentLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(tangent));
			if (tangentLength > 1e-4f)
				tangent = DirectX::XMVector3Normalize(tangent);
			else
				tangent = DirectX::XMVectorZero();

			float jt = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(relativeVelocity, tangent));
			jt /= denominator;

			float frictionCoefficient = 0.3f;
			float maxFriction = frictionCoefficient * normalImpulseMagnitude;
			jt = std::clamp(jt, -maxFriction, maxFriction);
			if (jt != 0.0f)
			{
				DirectX::XMVECTOR frictionImpulse = DirectX::XMVectorScale(tangent, jt);
				if (a->m_InvMass > 0.0f)
				{
					a->ApplyForce(Force{ DirectX::XMVectorNegate(frictionImpulse), 0.01f, contactPoint });
				}
				if (b->m_InvMass > 0.0f)
				{
					b->ApplyForce(Force{ frictionImpulse, 0.01f, contactPoint });
				}
			}
		}
	}
	DirectX::XMVECTOR PhysicsEngine::CalculateContactTangent(DirectX::XMVECTOR normal)
	{
		DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR right = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		// Use up if normal is not too close to Y axis
		DirectX::XMVECTOR tangent = DirectX::XMVector3Cross(normal, up);
		if (DirectX::XMVector3LengthSq(tangent).m128_f32[0] < 1e-4f) 
		{
			tangent = DirectX::XMVector3Cross(normal, right);
		}

		return DirectX::XMVector3Normalize(tangent);
	}
}
