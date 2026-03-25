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
			Step(FIXED_DT * SIMULATION_RATE);
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

	void PhysicsEngine::Step(float dt)
	{
		for (const auto& component : m_Components)
			component->IntegrateVelocity(dt);

		std::vector<ContactManifold> contacts;
		for (size_t i = 0; i < m_Components.size(); i++)
		{
			for (size_t j = i + 1; j < m_Components.size(); j++)
			{
				ContactManifold contact;
				if (CheckCollision(m_Components[i], m_Components[j], &contact))
				{
					contacts.push_back(contact);
				}
			}
		}

		for (const auto& component : m_Components)
		{
			component->m_PseudoVelocity = DirectX::XMVectorZero();
		}

		for (auto& contact : contacts)
		{
			if (contact.Contacts.size() > 0)
			{
				ResolveCollision(contact, dt);
			}
		}

		// Linear-only penetration correction via pseudo-velocities
		for (auto& contact : contacts)
		{
			if (contact.Contacts.empty()) continue;
			PhysicsComponent* a = contact.A;
			PhysicsComponent* b = contact.B;
			float totalInvMass = a->m_InvMass + b->m_InvMass;
			if (totalInvMass <= 0.0f) continue;

			float maxPen = 0.0f;
			for (const auto& cp : contact.Contacts)
				maxPen = (std::max)(maxPen, cp.PenetrationDepth);

			float correction = (std::max)(maxPen - PENETRATION_SLOP, 0.0f);
			if (correction > 0.0f)
			{
				float biasSpeed = (BAUMGARTE_FACTOR / dt) * correction;
				DirectX::XMVECTOR biasVel = DirectX::XMVectorScale(contact.Normal, biasSpeed);
				if (a->m_InvMass > 0.0f)
					a->m_PseudoVelocity = DirectX::XMVectorSubtract(a->m_PseudoVelocity,
						DirectX::XMVectorScale(biasVel, a->m_InvMass / totalInvMass));
				if (b->m_InvMass > 0.0f)
					b->m_PseudoVelocity = DirectX::XMVectorAdd(b->m_PseudoVelocity,
						DirectX::XMVectorScale(biasVel, b->m_InvMass / totalInvMass));
			}
		}

		for (auto it = m_ContactCache.begin(); it != m_ContactCache.end();)
		{
			it->second.Age++;
			if (it->second.Age > CONTACT_CACHE_MAX_AGE)
				it = m_ContactCache.erase(it);
			else
				++it;
		}

		for (const auto& component : m_Components)
			component->IntegratePosition(dt);

		for (const auto& component : m_Components)
		{
			if (component->m_InvMass > 0.0f)
			{
				if (!DirectX::XMVector3Equal(component->m_PseudoVelocity, DirectX::XMVectorZero()))
					component->m_Parent->Move(DirectX::XMVectorScale(component->m_PseudoVelocity, dt));
			}
		}
	}

	bool PhysicsEngine::CheckCollision(PhysicsComponent* a, PhysicsComponent* b, ContactManifold* outContact)
	{
		const CollisionMesh& meshA = a->GetCollisionMesh();
		const CollisionMesh& meshB = b->GetCollisionMesh();
		if (meshA.Type == CollisionMeshType::None || meshB.Type == CollisionMeshType::None)
			return false;

		if (a->GetBoundingBox().Intersects(b->GetBoundingBox()))
		{
			if (meshA.Intersects(meshB, outContact))
			{
				outContact->A = b->m_InvMass == 0.0f ? b : a;
				outContact->B = b->m_InvMass == 0.0f ? a : b;
				return true;
			}
		}
		return false;
	}

	void PhysicsEngine::ResolveCollision(ContactManifold& contact, float dt)
	{
		PhysicsComponent* a = contact.A;
		PhysicsComponent* b = contact.B;

		float restitution     = std::min(a->m_Restitution,    b->m_Restitution);
		float staticFriction  = std::sqrt(a->m_StaticFriction  * b->m_StaticFriction);
		float kineticFriction = std::sqrt(a->m_KineticFriction * b->m_KineticFriction);

		constexpr float kRestitutionSlop = 0.5f;

		const int numContacts = static_cast<int>(contact.Contacts.size());
		if (numContacts == 0) return;

		DirectX::XMVECTOR tangent0, tangent1;
		ComputeFrictionBasis(contact.Normal, tangent0, tangent1);

		std::vector<float> accJn(numContacts, 0.0f);
		std::vector<float> accJt0(numContacts, 0.0f);
		std::vector<float> accJt1(numContacts, 0.0f);

		bool isContinuingContact = WarmStart(contact, accJn, accJt0, accJt1, tangent0, tangent1);

		for (int iter = 0; iter < SOLVER_ITERATIONS; ++iter)
		{
			for (int ci = 0; ci < numContacts; ++ci)
			{
				const ContactPoint& cp = contact.Contacts[ci];
				DirectX::XMVECTOR ra = DirectX::XMVectorSubtract(cp.Point, a->GetPosition());
				DirectX::XMVECTOR rb = DirectX::XMVectorSubtract(cp.Point, b->GetPosition());

				// --- Normal constraint (Gauss-Seidel: reads current velocities) ---
				DirectX::XMVECTOR contactVelA = DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, ra));
				DirectX::XMVECTOR contactVelB = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, rb));
				DirectX::XMVECTOR relVel = DirectX::XMVectorSubtract(contactVelB, contactVelA);

				float vn = DirectX::XMVectorGetX(DirectX::XMVector3Dot(relVel, contact.Normal));
				DirectX::XMVECTOR raCrossN = DirectX::XMVector3Cross(ra, contact.Normal);
				DirectX::XMVECTOR rbCrossN = DirectX::XMVector3Cross(rb, contact.Normal);
				float angTermA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
					DirectX::XMVector3Cross(DirectX::XMVector3Transform(raCrossN, a->m_InverseInertiaTensor), ra), contact.Normal));
				float angTermB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
					DirectX::XMVector3Cross(DirectX::XMVector3Transform(rbCrossN, b->m_InverseInertiaTensor), rb), contact.Normal));
				float effMassN = a->m_InvMass + b->m_InvMass + angTermA + angTermB;
				if (effMassN <= 0.0f) continue;

				float e = (iter == 0 && !isContinuingContact && vn < -kRestitutionSlop) ? restitution : 0.0f;

				float jn = -(1.0f + e) * vn / effMassN;
				float oldAccJn = accJn[ci];
				accJn[ci] = (std::max)(oldAccJn + jn, 0.0f);
				jn = accJn[ci] - oldAccJn;

				if (jn != 0.0f)
				{
					DirectX::XMVECTOR impulse = DirectX::XMVectorScale(contact.Normal, jn);
					if (a->m_InvMass > 0.0f)
					{
						a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(impulse, a->m_InvMass));
						a->m_AngularVelocity = DirectX::XMVectorSubtract(a->m_AngularVelocity,
							DirectX::XMVector3Transform(DirectX::XMVectorScale(raCrossN, jn), a->m_InverseInertiaTensor));
					}
					if (b->m_InvMass > 0.0f)
					{
						b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(impulse, b->m_InvMass));
						b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity,
							DirectX::XMVector3Transform(DirectX::XMVectorScale(rbCrossN, jn), b->m_InverseInertiaTensor));
					}
				}

				// --- Friction constraints (re-read velocities after normal impulse) ---
				contactVelA = DirectX::XMVectorAdd(a->m_Velocity, DirectX::XMVector3Cross(a->m_AngularVelocity, ra));
				contactVelB = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVector3Cross(b->m_AngularVelocity, rb));
				relVel = DirectX::XMVectorSubtract(contactVelB, contactVelA);

				float maxFriction = staticFriction * accJn[ci];

				// Tangent0
				{
					float vt = DirectX::XMVectorGetX(DirectX::XMVector3Dot(relVel, tangent0));
					DirectX::XMVECTOR raCrossT = DirectX::XMVector3Cross(ra, tangent0);
					DirectX::XMVECTOR rbCrossT = DirectX::XMVector3Cross(rb, tangent0);
					float angTermTA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
						DirectX::XMVector3Cross(DirectX::XMVector3Transform(raCrossT, a->m_InverseInertiaTensor), ra), tangent0));
					float angTermTB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
						DirectX::XMVector3Cross(DirectX::XMVector3Transform(rbCrossT, b->m_InverseInertiaTensor), rb), tangent0));
					float effMassT = a->m_InvMass + b->m_InvMass + angTermTA + angTermTB;
					if (effMassT > 0.0f)
					{
						float lambda = -vt / effMassT;
						float oldAcc = accJt0[ci];
						accJt0[ci] = (std::max)(-maxFriction, (std::min)(oldAcc + lambda, maxFriction));
						lambda = accJt0[ci] - oldAcc;
						if (lambda != 0.0f)
						{
							DirectX::XMVECTOR imp = DirectX::XMVectorScale(tangent0, lambda);
							if (a->m_InvMass > 0.0f)
							{
								a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(imp, a->m_InvMass));
								a->m_AngularVelocity = DirectX::XMVectorSubtract(a->m_AngularVelocity,
									DirectX::XMVector3Transform(DirectX::XMVectorScale(raCrossT, lambda), a->m_InverseInertiaTensor));
							}
							if (b->m_InvMass > 0.0f)
							{
								b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(imp, b->m_InvMass));
								b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity,
									DirectX::XMVector3Transform(DirectX::XMVectorScale(rbCrossT, lambda), b->m_InverseInertiaTensor));
							}
						}
					}
				}

				// Tangent1
				{
					float vt = DirectX::XMVectorGetX(DirectX::XMVector3Dot(relVel, tangent1));
					DirectX::XMVECTOR raCrossT = DirectX::XMVector3Cross(ra, tangent1);
					DirectX::XMVECTOR rbCrossT = DirectX::XMVector3Cross(rb, tangent1);
					float angTermTA = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
						DirectX::XMVector3Cross(DirectX::XMVector3Transform(raCrossT, a->m_InverseInertiaTensor), ra), tangent1));
					float angTermTB = DirectX::XMVectorGetX(DirectX::XMVector3Dot(
						DirectX::XMVector3Cross(DirectX::XMVector3Transform(rbCrossT, b->m_InverseInertiaTensor), rb), tangent1));
					float effMassT = a->m_InvMass + b->m_InvMass + angTermTA + angTermTB;
					if (effMassT > 0.0f)
					{
						float lambda = -vt / effMassT;
						float oldAcc = accJt1[ci];
						accJt1[ci] = (std::max)(-maxFriction, (std::min)(oldAcc + lambda, maxFriction));
						lambda = accJt1[ci] - oldAcc;
						if (lambda != 0.0f)
						{
							DirectX::XMVECTOR imp = DirectX::XMVectorScale(tangent1, lambda);
							if (a->m_InvMass > 0.0f)
							{
								a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(imp, a->m_InvMass));
								a->m_AngularVelocity = DirectX::XMVectorSubtract(a->m_AngularVelocity,
									DirectX::XMVector3Transform(DirectX::XMVectorScale(raCrossT, lambda), a->m_InverseInertiaTensor));
							}
							if (b->m_InvMass > 0.0f)
							{
								b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(imp, b->m_InvMass));
								b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity,
									DirectX::XMVector3Transform(DirectX::XMVectorScale(rbCrossT, lambda), b->m_InverseInertiaTensor));
							}
						}
					}
				}
			}
		}

		StoreCache(contact, accJn, accJt0, accJt1, tangent0, tangent1);
	}

	void PhysicsEngine::ComputeFrictionBasis(DirectX::XMVECTOR normal, DirectX::XMVECTOR& outTangent0, DirectX::XMVECTOR& outTangent1)
	{
		DirectX::XMVECTOR ref = (std::abs(DirectX::XMVectorGetY(normal)) < 0.9f)
			? DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
			: DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		outTangent0 = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(normal, ref));
		outTangent1 = DirectX::XMVector3Cross(normal, outTangent0);
	}

	BodyPairKey PhysicsEngine::MakeKey(PhysicsComponent* a, PhysicsComponent* b)
	{
		if (a < b) return { a, b };
		return { b, a };
	}

	bool PhysicsEngine::WarmStart(ContactManifold& contact, std::vector<float>& accJn, std::vector<float>& accJt0,
		std::vector<float>& accJt1, DirectX::XMVECTOR tangent0, DirectX::XMVECTOR tangent1)
	{
		BodyPairKey key = MakeKey(contact.A, contact.B);
		auto it = m_ContactCache.find(key);
		if (it == m_ContactCache.end()) return false;

		CachedManifold& cached = it->second;
		PhysicsComponent* a = contact.A;
		PhysicsComponent* b = contact.B;
		const int numContacts = static_cast<int>(contact.Contacts.size());

		for (int ci = 0; ci < numContacts; ++ci)
		{
			const ContactPoint& cp = contact.Contacts[ci];
			int bestIdx = -1;
			float bestDistSq = CONTACT_MATCH_THRESHOLD_SQ;
			for (int j = 0; j < static_cast<int>(cached.Contacts.size()); ++j)
			{
				float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(
					DirectX::XMVectorSubtract(cp.Point, cached.Contacts[j].Point)));
				if (distSq < bestDistSq) { bestDistSq = distSq; bestIdx = j; }
			}
			if (bestIdx < 0) continue;

			const CachedContact& cc = cached.Contacts[bestIdx];
			DirectX::XMVECTOR oldTangentImpulse = DirectX::XMVectorAdd(
				DirectX::XMVectorScale(cached.Tangent0, cc.AccJt0),
				DirectX::XMVectorScale(cached.Tangent1, cc.AccJt1));
			float warmJn  = cc.AccJn * WARM_START_FACTOR;
			float warmJt0 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(oldTangentImpulse, tangent0)) * WARM_START_FACTOR;
			float warmJt1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(oldTangentImpulse, tangent1)) * WARM_START_FACTOR;
			accJn[ci] = warmJn; accJt0[ci] = warmJt0; accJt1[ci] = warmJt1;

			DirectX::XMVECTOR ra = DirectX::XMVectorSubtract(cp.Point, a->GetPosition());
			DirectX::XMVECTOR rb = DirectX::XMVectorSubtract(cp.Point, b->GetPosition());
			DirectX::XMVECTOR totalImpulse = DirectX::XMVectorAdd(
				DirectX::XMVectorScale(contact.Normal, warmJn),
				DirectX::XMVectorAdd(DirectX::XMVectorScale(tangent0, warmJt0), DirectX::XMVectorScale(tangent1, warmJt1)));

			if (a->m_InvMass > 0.0f)
			{
				a->m_Velocity = DirectX::XMVectorSubtract(a->m_Velocity, DirectX::XMVectorScale(totalImpulse, a->m_InvMass));
				DirectX::XMVECTOR angA = DirectX::XMVectorAdd(
					DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(ra, contact.Normal), warmJn), a->m_InverseInertiaTensor),
					DirectX::XMVectorAdd(
						DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(ra, tangent0), warmJt0), a->m_InverseInertiaTensor),
						DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(ra, tangent1), warmJt1), a->m_InverseInertiaTensor)));
				a->m_AngularVelocity = DirectX::XMVectorSubtract(a->m_AngularVelocity, angA);
			}
			if (b->m_InvMass > 0.0f)
			{
				b->m_Velocity = DirectX::XMVectorAdd(b->m_Velocity, DirectX::XMVectorScale(totalImpulse, b->m_InvMass));
				DirectX::XMVECTOR angB = DirectX::XMVectorAdd(
					DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(rb, contact.Normal), warmJn), b->m_InverseInertiaTensor),
					DirectX::XMVectorAdd(
						DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(rb, tangent0), warmJt0), b->m_InverseInertiaTensor),
						DirectX::XMVector3Transform(DirectX::XMVectorScale(DirectX::XMVector3Cross(rb, tangent1), warmJt1), b->m_InverseInertiaTensor)));
				b->m_AngularVelocity = DirectX::XMVectorAdd(b->m_AngularVelocity, angB);
			}
		}
		return true;
	}

	void PhysicsEngine::StoreCache(const ContactManifold& contact, const std::vector<float>& accJn,
		const std::vector<float>& accJt0, const std::vector<float>& accJt1,
		DirectX::XMVECTOR tangent0, DirectX::XMVECTOR tangent1)
	{
		BodyPairKey key = MakeKey(contact.A, contact.B);
		CachedManifold cached;
		cached.Normal = contact.Normal;
		cached.Tangent0 = tangent0;
		cached.Tangent1 = tangent1;
		cached.Age = 0;
		const int numContacts = static_cast<int>(contact.Contacts.size());
		cached.Contacts.resize(numContacts);
		for (int ci = 0; ci < numContacts; ++ci)
		{
			cached.Contacts[ci].Point = contact.Contacts[ci].Point;
			cached.Contacts[ci].AccJn = accJn[ci];
			cached.Contacts[ci].AccJt0 = accJt0[ci];
			cached.Contacts[ci].AccJt1 = accJt1[ci];
		}
		m_ContactCache[key] = cached;
	}
}
