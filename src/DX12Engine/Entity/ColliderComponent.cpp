#include "ColliderComponent.h"
#include "GameObject.h"
#include "RenderComponent.h"
#include "../Asset/ModelAsset.h"
#include "../Asset/MeshAsset.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

namespace DX12Engine
{
	namespace
	{
		DirectX::XMFLOAT3 ToFloat3(DirectX::FXMVECTOR v)
		{
			DirectX::XMFLOAT3 result;
			DirectX::XMStoreFloat3(&result, v);
			return result;
		}

		void ComputeAABBFromOBB(
			DirectX::FXMVECTOR center,
			const std::array<DirectX::XMVECTOR, 3>& axes,
			DirectX::FXMVECTOR extents,
			DirectX::XMFLOAT3& outMin,
			DirectX::XMFLOAT3& outMax)
		{
			const DirectX::XMFLOAT3 c = ToFloat3(center);
			const DirectX::XMFLOAT3 a0 = ToFloat3(axes[0]);
			const DirectX::XMFLOAT3 a1 = ToFloat3(axes[1]);
			const DirectX::XMFLOAT3 a2 = ToFloat3(axes[2]);

			const float ex = DirectX::XMVectorGetX(extents);
			const float ey = DirectX::XMVectorGetY(extents);
			const float ez = DirectX::XMVectorGetZ(extents);

			const float halfX = std::fabs(a0.x) * ex + std::fabs(a1.x) * ey + std::fabs(a2.x) * ez;
			const float halfY = std::fabs(a0.y) * ex + std::fabs(a1.y) * ey + std::fabs(a2.y) * ez;
			const float halfZ = std::fabs(a0.z) * ex + std::fabs(a1.z) * ey + std::fabs(a2.z) * ez;

			outMin = { c.x - halfX, c.y - halfY, c.z - halfZ };
			outMax = { c.x + halfX, c.y + halfY, c.z + halfZ };
		}
	}

	ColliderComponent::ColliderComponent(GameObject* parent)
		: Component(parent, ComponentType::Collider, true), m_CollisionMeshType(CollisionMeshType::None)
	{
		SetDefaultLocalShape(CollisionMeshType::None);
		RefreshCollisionData(true);
	}

	ColliderComponent::ColliderComponent(GameObject* parent, CollisionMeshType type)
		: Component(parent, ComponentType::Collider, true), m_CollisionMeshType(type)
	{
		SetDefaultLocalShape(type);
		RefreshCollisionData(true);
	}

	ColliderComponent::~ColliderComponent()
	{
	}

	void ColliderComponent::Init()
	{
	}

	void ColliderComponent::Update(float ts, float elapsed)
	{
	}

	void ColliderComponent::OnTransformChanged(TransformType type)
	{
		RefreshCollisionData(true);
	}

	void ColliderComponent::SetCollisionType(CollisionMeshType type)
	{
		m_CollisionMeshType = type;
		if (!m_UseRenderModelForCollision)
			SetDefaultLocalShape(type);
		RefreshCollisionData(true);
	}

	bool ColliderComponent::UseRenderModelForCollision()
	{
		RenderComponent* renderComponent = m_Parent ? m_Parent->GetComponent<RenderComponent>() : nullptr;
		if (!renderComponent)
			return false;

		ModelInstance* modelInstance = renderComponent->GetAsset();
		return BuildCollisionFromModel(modelInstance);
	}

	void ColliderComponent::SetUseRenderModelForCollision(bool useRenderModel)
	{
		m_UseRenderModelForCollision = useRenderModel;
		if (!m_UseRenderModelForCollision)
			SetDefaultLocalShape(m_CollisionMeshType);
		RefreshCollisionData(true);
	}

	bool ColliderComponent::BuildCollisionFromModel(ModelInstance* modelInstance)
	{
		if (!modelInstance)
			return false;

		ModelAsset* modelAsset = modelInstance->GetModelAsset();
		if (!modelAsset)
			return false;

		DirectX::XMFLOAT3 minBounds = { FLT_MAX, FLT_MAX, FLT_MAX };
		DirectX::XMFLOAT3 maxBounds = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		bool hasBounds = false;

		for (const auto& meshAsset : modelAsset->GetMeshes())
		{
			if (!meshAsset)
				continue;

			for (const MeshPrimitive& primitive : meshAsset->GetPrimitives())
			{
				const MeshBounds& primitiveBounds = primitive.GetBounds();
				minBounds.x = (std::min)(minBounds.x, primitiveBounds.Min.x);
				minBounds.y = (std::min)(minBounds.y, primitiveBounds.Min.y);
				minBounds.z = (std::min)(minBounds.z, primitiveBounds.Min.z);
				maxBounds.x = (std::max)(maxBounds.x, primitiveBounds.Max.x);
				maxBounds.y = (std::max)(maxBounds.y, primitiveBounds.Max.y);
				maxBounds.z = (std::max)(maxBounds.z, primitiveBounds.Max.z);
				hasBounds = true;
			}
		}

		if (!hasBounds)
			return false;

		const float sizeX = maxBounds.x - minBounds.x;
		const float sizeY = maxBounds.y - minBounds.y;
		const float sizeZ = maxBounds.z - minBounds.z;
		const CollisionMeshType targetType = (m_CollisionMeshType == CollisionMeshType::None)
												 ? CollisionMeshType::Box
												 : m_CollisionMeshType;

		switch (targetType)
		{
		case CollisionMeshType::Box:
			m_LocalHalfExtents = { 0.5f * sizeX, 0.5f * sizeY, 0.5f * sizeZ };
			break;
		case CollisionMeshType::Sphere:
			m_LocalSphereRadius = 0.5f * (std::max)(sizeX, (std::max)(sizeY, sizeZ));
			m_LocalHalfExtents = { m_LocalSphereRadius, m_LocalSphereRadius, m_LocalSphereRadius };
			break;
		case CollisionMeshType::Plane:
			m_LocalHalfExtents = { 0.5f * sizeX, 0.01f, 0.5f * sizeZ };
			break;
		case CollisionMeshType::None:
		default:
			return false;
		}

		m_CollisionMeshType = targetType;
		m_CollisionMesh.Type = targetType;
		return true;
	}

	void ColliderComponent::RefreshCollisionData(bool notifyListeners)
	{
		if (m_UseRenderModelForCollision)
		{
			if (!UseRenderModelForCollision() && m_CollisionMeshType == CollisionMeshType::None)
				SetDefaultLocalShape(CollisionMeshType::Box);
		}

		SyncCollisionToTransform();
		UpdateBoundingBox();

		if (notifyListeners)
			NotifyChanged();
	}

	void ColliderComponent::SyncCollisionToTransform()
	{
		if (!m_Parent)
			return;

		const DirectX::XMVECTOR scale = m_Parent->GetScale();
		const float absScaleX = std::fabs(DirectX::XMVectorGetX(scale));
		const float absScaleY = std::fabs(DirectX::XMVectorGetY(scale));
		const float absScaleZ = std::fabs(DirectX::XMVectorGetZ(scale));

		switch (m_CollisionMeshType)
		{
		case CollisionMeshType::Box:
		{
			DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation());
			m_CollisionMesh.Type = CollisionMeshType::Box;
			m_CollisionMesh.OBBData.Center = m_Parent->GetPosition();
			m_CollisionMesh.OBBData.Axis[0] = DirectX::XMVector3Normalize(rotationMatrix.r[0]);
			m_CollisionMesh.OBBData.Axis[1] = DirectX::XMVector3Normalize(rotationMatrix.r[1]);
			m_CollisionMesh.OBBData.Axis[2] = DirectX::XMVector3Normalize(rotationMatrix.r[2]);
			m_CollisionMesh.OBBData.Extents = DirectX::XMVectorSet(
				m_LocalHalfExtents.x * absScaleX,
				m_LocalHalfExtents.y * absScaleY,
				m_LocalHalfExtents.z * absScaleZ,
				0.0f);
			break;
		}
		case CollisionMeshType::Sphere:
		{
			const float maxScale = (std::max)(absScaleX, (std::max)(absScaleY, absScaleZ));
			m_CollisionMesh.Type = CollisionMeshType::Sphere;
			m_CollisionMesh.SphereData.Center = m_Parent->GetPosition();
			m_CollisionMesh.SphereData.Radius = m_LocalSphereRadius * maxScale;
			break;
		}
		case CollisionMeshType::Plane:
		{
			DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation());
			m_CollisionMesh.Type = CollisionMeshType::Plane;
			m_CollisionMesh.PlaneData.Center = m_Parent->GetPosition();
			m_CollisionMesh.PlaneData.Normal = DirectX::XMVector3Normalize(rotationMatrix.r[1]);
			m_CollisionMesh.PlaneData.Tangent0 = DirectX::XMVector3Normalize(rotationMatrix.r[0]);
			m_CollisionMesh.PlaneData.Tangent1 = DirectX::XMVector3Normalize(rotationMatrix.r[2]);
			m_CollisionMesh.PlaneData.HalfExtent0 = m_LocalHalfExtents.x * absScaleX;
			m_CollisionMesh.PlaneData.HalfExtent1 = m_LocalHalfExtents.z * absScaleZ;
			break;
		}
		case CollisionMeshType::None:
		default:
			m_CollisionMesh.Type = CollisionMeshType::None;
			break;
		}
	}

	void ColliderComponent::UpdateBoundingBox()
	{
		switch (m_CollisionMesh.Type)
		{
		case CollisionMeshType::Box:
		{
			DirectX::XMFLOAT3 minPoint;
			DirectX::XMFLOAT3 maxPoint;
			ComputeAABBFromOBB(
				m_CollisionMesh.OBBData.Center,
				{ m_CollisionMesh.OBBData.Axis[0], m_CollisionMesh.OBBData.Axis[1], m_CollisionMesh.OBBData.Axis[2] },
				m_CollisionMesh.OBBData.Extents,
				minPoint,
				maxPoint);
			SetBoundingBoxFromMinMax(minPoint, maxPoint);
			break;
		}
		case CollisionMeshType::Sphere:
		{
			const DirectX::XMFLOAT3 center = ToFloat3(m_CollisionMesh.SphereData.Center);
			const float radius = m_CollisionMesh.SphereData.Radius;
			SetBoundingBoxFromMinMax(
				{ center.x - radius, center.y - radius, center.z - radius },
				{ center.x + radius, center.y + radius, center.z + radius });
			break;
		}
		case CollisionMeshType::Plane:
		{
			const DirectX::XMVECTOR scale = m_Parent ? m_Parent->GetScale() : DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 0.0f);
			const float absScaleY = std::fabs(DirectX::XMVectorGetY(scale));
			const float halfThickness = (std::max)(0.005f, m_LocalHalfExtents.y * absScaleY);

			DirectX::XMFLOAT3 minPoint;
			DirectX::XMFLOAT3 maxPoint;
			ComputeAABBFromOBB(
				m_CollisionMesh.PlaneData.Center,
				{ m_CollisionMesh.PlaneData.Tangent0, m_CollisionMesh.PlaneData.Normal, m_CollisionMesh.PlaneData.Tangent1 },
				DirectX::XMVectorSet(
					m_CollisionMesh.PlaneData.HalfExtent0,
					halfThickness,
					m_CollisionMesh.PlaneData.HalfExtent1,
					0.0f),
				minPoint,
				maxPoint);
			SetBoundingBoxFromMinMax(minPoint, maxPoint);
			break;
		}
		case CollisionMeshType::None:
		default:
			m_BoundingBox = {};
			break;
		}
	}

	void ColliderComponent::SetBoundingBoxFromMinMax(const DirectX::XMFLOAT3& minPoint, const DirectX::XMFLOAT3& maxPoint)
	{
		m_BoundingBox.MinPoint = minPoint;
		m_BoundingBox.MaxPoint = maxPoint;
		m_BoundingBox.Dimensions = {
			maxPoint.x - minPoint.x,
			maxPoint.y - minPoint.y,
			maxPoint.z - minPoint.z
		};

		m_BoundingBox.Vertices = {
			DirectX::XMVectorSet(minPoint.x, minPoint.y, minPoint.z, 1.0f),
			DirectX::XMVectorSet(maxPoint.x, minPoint.y, minPoint.z, 1.0f),
			DirectX::XMVectorSet(maxPoint.x, maxPoint.y, minPoint.z, 1.0f),
			DirectX::XMVectorSet(minPoint.x, maxPoint.y, minPoint.z, 1.0f),
			DirectX::XMVectorSet(minPoint.x, minPoint.y, maxPoint.z, 1.0f),
			DirectX::XMVectorSet(maxPoint.x, minPoint.y, maxPoint.z, 1.0f),
			DirectX::XMVectorSet(maxPoint.x, maxPoint.y, maxPoint.z, 1.0f),
			DirectX::XMVectorSet(minPoint.x, maxPoint.y, maxPoint.z, 1.0f)
		};

		if (m_BoundingBox.Indices.empty())
		{
			m_BoundingBox.Indices = {
				0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6, 0, 3, 7, 0, 7, 4, 1, 2, 6, 1, 6, 5
			};
		}
	}

	void ColliderComponent::SetDefaultLocalShape(CollisionMeshType type)
	{
		switch (type)
		{
		case CollisionMeshType::Box:
			m_LocalHalfExtents = { 0.5f, 0.5f, 0.5f };
			break;
		case CollisionMeshType::Sphere:
			m_LocalSphereRadius = 0.5f;
			m_LocalHalfExtents = { m_LocalSphereRadius, m_LocalSphereRadius, m_LocalSphereRadius };
			break;
		case CollisionMeshType::Plane:
			m_LocalHalfExtents = { 0.5f, 0.01f, 0.5f };
			break;
		case CollisionMeshType::None:
		default:
			m_LocalHalfExtents = { 0.0f, 0.0f, 0.0f };
			m_LocalSphereRadius = 0.0f;
			break;
		}

		m_CollisionMesh.Type = type;
	}

	void ColliderComponent::NotifyChanged()
	{
		if (m_Parent != nullptr)
			m_Parent->DispatchColliderChanged(this);
	}
}
