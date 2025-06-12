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
					//std::cout << "Collision detected between component " << i << " and component " << j << std::endl;
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
		if (velocityAlongNormal < 0.1f)
		{
			float restitution = 50.0f;
			float impulseMagnitude = -(1.0f + restitution) * velocityAlongNormal;
			impulseMagnitude /= (a->m_InvMass + b->m_InvMass);
			DirectX::XMVECTOR impulse = DirectX::XMVectorScale(contact.Normal, impulseMagnitude);
			if (a->m_InvMass > 0.0f)
				a->ApplyForce(Force{ DirectX::XMVectorNegate(impulse), 0.01f, contactPoint });
			if (b->m_InvMass > 0.0f)
				b->ApplyForce(Force{ impulse, 0.01f, contactPoint });
		}
	}
}
