#pragma once
#include "Component.h"
#include "../Physics/AABoundingBox.h"
#include "../Physics/CollisionMesh.h"

namespace DX12Engine
{
	class ColliderComponent : public Component
	{
	public:
		ColliderComponent(GameObject* parent);
		ColliderComponent(GameObject* parent, CollisionMeshType type);
		~ColliderComponent() override;

		void Init() override;
		void Update(float ts, float elapsed) override;

		void OnTransformChanged(TransformType type) override;

		CollisionMeshType GetCollisionType() const { return m_CollisionMeshType; }
		const CollisionMesh& GetCollisionMesh() const { return m_CollisionMesh; }
		const AABoundingBox& GetBoundingBox() const { return m_BoundingBox; }

		void SetCollisionType(CollisionMeshType type);
		void SetUseRenderModelForCollision(bool useRenderModel);
		bool IsUsingRenderModelForCollision() const { return m_UseRenderModelForCollision; }

	private:
		void RefreshCollisionData(bool notifyListeners);
		void SyncCollisionToTransform();
		void UpdateBoundingBox();
		void SetBoundingBoxFromMinMax(const DirectX::XMFLOAT3& minPoint, const DirectX::XMFLOAT3& maxPoint);
		void SetDefaultLocalShape(CollisionMeshType type);
		void NotifyChanged();
		bool UseRenderModelForCollision();
		bool BuildCollisionFromModel(ModelInstance* modelInstance);

		CollisionMeshType m_CollisionMeshType = CollisionMeshType::None;
		CollisionMesh m_CollisionMesh;
		AABoundingBox m_BoundingBox;
		DirectX::XMFLOAT3 m_LocalHalfExtents = { 0.5f, 0.5f, 0.5f };
		float m_LocalSphereRadius = 0.5f;
		bool m_UseRenderModelForCollision = false;
	};
}
