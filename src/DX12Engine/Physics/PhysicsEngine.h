#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include "../Entity/PhysicsComponent.h"

constexpr float SIMULATION_RATE = 1.0f;
constexpr float FIXED_DT = 1.0f / 120.0f;
constexpr int MAX_SUBSTEPS = 8;
constexpr int SOLVER_ITERATIONS = 8;
constexpr float BAUMGARTE_FACTOR = 0.2f;
constexpr float PENETRATION_SLOP = 0.005f;
constexpr float WARM_START_FACTOR = 0.85f;
constexpr float CONTACT_MATCH_THRESHOLD_SQ = 0.02f * 0.02f;
constexpr int CONTACT_CACHE_MAX_AGE = 3;

namespace DX12Engine
{
	class PhysicsEngine
	{
	public:
		PhysicsEngine() = default;
		~PhysicsEngine() = default;

		void Update(float ts, float elapsed);

		void SetComponents(std::vector<PhysicsComponent*> components);

	private:
		void Step(float dt);
		bool CheckCollision(PhysicsComponent* a, PhysicsComponent* b, ContactManifold* outContact);
		void ResolveCollision(ContactManifold& contact, float dt);
		void ComputeFrictionBasis(DirectX::XMVECTOR normal, DirectX::XMVECTOR& outTangent0, DirectX::XMVECTOR& outTangent1);

		BodyPairKey MakeKey(PhysicsComponent* a, PhysicsComponent* b);
		bool WarmStart(ContactManifold& contact, std::vector<float>& accJn, std::vector<float>& accJt0, std::vector<float>& accJt1,
			DirectX::XMVECTOR tangent0, DirectX::XMVECTOR tangent1);
		void StoreCache(const ContactManifold& contact, const std::vector<float>& accJn, const std::vector<float>& accJt0,
			const std::vector<float>& accJt1, DirectX::XMVECTOR tangent0, DirectX::XMVECTOR tangent1);

		std::vector<PhysicsComponent*> m_Components;
		std::unordered_map<BodyPairKey, CachedManifold, BodyPairHash> m_ContactCache;
		float m_Accumulator = 0.0f;
	};
}

