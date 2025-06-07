#define NOMINMAX
#include "PhysicsComponent.h"
#include "GameObject.h"
#include "../Utils/EngineUtils.h"
#include <iostream>

namespace DX12Engine
{
    PhysicsComponent::PhysicsComponent(GameObject* parent)
		: Component(parent, ComponentType::Physics, true),
		m_Velocity(DirectX::XMVectorZero()),
		m_AngularVelocity(DirectX::XMVectorZero()),
		m_Acceleration(DirectX::XMVectorZero()),
		m_Torque(DirectX::XMVectorZero()),
		m_InverseInertiaTensor(DirectX::XMMatrixIdentity()),
		m_LocalInertiaTensor(DirectX::XMMatrixIdentity()),
		m_Mass(1.0f),
		m_InvMass(1.0f),
		m_IsStatic(false)
    {
		OnMeshChanged(parent->GetMesh());
    }

    PhysicsComponent::~PhysicsComponent()
    {
    }

    void PhysicsComponent::Init()
    {
    }

    void PhysicsComponent::Update(float ts, float elapsed)
    {
		if (m_IsStatic) return;

		EvaluateForces(ts);
		m_Velocity = DirectX::XMVectorAdd(m_Velocity, DirectX::XMVectorScale(m_Acceleration, ts));

		if (!DirectX::XMVector4Equal(m_Torque, DirectX::XMVectorZero()) || !DirectX::XMVector4Equal(m_AngularVelocity, DirectX::XMVectorZero()))
		{
			DirectX::XMVECTOR angularAccceleration = DirectX::XMVector3Transform(m_Torque, m_InverseInertiaTensor);
			m_AngularVelocity = DirectX::XMVectorAdd(m_AngularVelocity, DirectX::XMVectorScale(angularAccceleration, ts));
			UpdateInertiaTensor();

			DirectX::XMVECTOR rotation = m_Parent->GetRotation();
			DirectX::XMVECTOR delta = DirectX::XMQuaternionMultiply(rotation, DirectX::XMVectorScale(m_AngularVelocity, 0.5f));
			rotation = DirectX::XMVector4Normalize(DirectX::XMVectorAdd(rotation, DirectX::XMVectorScale(delta, ts)));
			m_Parent->SetRotationQuaternion(rotation);
		}

		m_Parent->Move(DirectX::XMVectorScale(m_Velocity, ts));

		m_Acceleration = DirectX::XMVectorZero();
		m_Torque = DirectX::XMVectorZero();
    }

	void PhysicsComponent::OnMeshChanged(Mesh* newMesh)
	{
		if (newMesh)
		{
			std::vector<DirectX::XMVECTOR> vertexPositions;
			for (const auto& vertex : newMesh->Vertices)
				vertexPositions.push_back(DirectX::XMLoadFloat3(&vertex.Position));
			m_BoundingBox.Vertices = GetBoundingBoxVertices(vertexPositions);
			m_BoundingBox.Indices = {
				0, 1, 2, 0, 2, 3,
				4, 5, 6, 4, 6, 7,
				0, 1, 5, 0, 5, 4,
				2, 3, 7, 2, 7, 6,
				0, 3, 7, 0, 7, 4,
				1, 2, 6, 1, 6, 5
			};
			m_BoundingBox.MinPoint = { DirectX::XMVectorGetX(m_BoundingBox.Vertices[0]), DirectX::XMVectorGetY(m_BoundingBox.Vertices[0]), DirectX::XMVectorGetZ(m_BoundingBox.Vertices[0]) };
			m_BoundingBox.MaxPoint = { DirectX::XMVectorGetX(m_BoundingBox.Vertices[6]), DirectX::XMVectorGetY(m_BoundingBox.Vertices[6]), DirectX::XMVectorGetZ(m_BoundingBox.Vertices[6]) };
			m_BoundingBox.Dimensions = {
				m_BoundingBox.MaxPoint.x - m_BoundingBox.MinPoint.x,
				m_BoundingBox.MaxPoint.y - m_BoundingBox.MinPoint.y,
				m_BoundingBox.MaxPoint.z - m_BoundingBox.MinPoint.z
			};
			m_CollisionMesh.SphereData.Radius = (DirectX::XMVectorGetX(m_BoundingBox.Vertices[6]) - DirectX::XMVectorGetX(m_BoundingBox.Vertices[0])) / 2.0f;
			UpdateCollisionMesh();
		}
	}

	void PhysicsComponent::OnTransformChanged(TransformType type)
	{
		switch (m_CollisionMesh.Type)
		{
		case CollisionMeshType::Box:
		{
			std::vector<DirectX::XMVECTOR> transformedVertices;
			DirectX::XMMATRIX modelMatrix = m_Parent->GetModelMatrix();
			for (auto& vertex : m_BoundingBox.Vertices)
				transformedVertices.push_back(DirectX::XMVector3Transform(vertex, modelMatrix));
			std::vector<DirectX::XMVECTOR> updatedVertices = GetBoundingBoxVertices(transformedVertices);
			m_BoundingBox.MinPoint = { DirectX::XMVectorGetX(updatedVertices[0]), DirectX::XMVectorGetY(updatedVertices[0]), DirectX::XMVectorGetZ(updatedVertices[0]) };
			m_BoundingBox.MaxPoint = { DirectX::XMVectorGetX(updatedVertices[6]), DirectX::XMVectorGetY(updatedVertices[6]), DirectX::XMVectorGetZ(updatedVertices[6]) };

			DirectX::XMVECTOR right = DirectX::XMVector3Normalize(modelMatrix.r[0]);
			DirectX::XMVECTOR up = DirectX::XMVector3Normalize(modelMatrix.r[1]);
			DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(modelMatrix.r[2]);
			DirectX::XMVECTOR localExtents = DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&m_BoundingBox.MinPoint), DirectX::XMLoadFloat3(&m_BoundingBox.MaxPoint)), 0.5f);
			DirectX::XMVECTOR worldExtents;
			worldExtents = DirectX::XMVectorSet(
				DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(modelMatrix.r[0], DirectX::XMVectorGetX(localExtents)))),
				DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(modelMatrix.r[1], DirectX::XMVectorGetY(localExtents)))),
				DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorScale(modelMatrix.r[2], DirectX::XMVectorGetZ(localExtents)))),
				0.0f
			);
			m_CollisionMesh.OBBData.Center = m_Parent->GetPosition();
			m_CollisionMesh.OBBData.Axis[0] = right;
			m_CollisionMesh.OBBData.Axis[1] = up;
			m_CollisionMesh.OBBData.Axis[2] = forward;
			m_CollisionMesh.OBBData.Extents = worldExtents;
			break;
		}
		case CollisionMeshType::Sphere:
		{
			DirectX::XMFLOAT3 position = EngineUtils::ConvertToXMFLOAT3(m_Parent->GetPosition());
			m_BoundingBox.MinPoint = { position.x - m_BoundingBox.Dimensions.x / 2.0f, position.y - m_BoundingBox.Dimensions.y / 2.0f, position.z - m_BoundingBox.Dimensions.z / 2.0f };
			m_BoundingBox.MaxPoint = { position.x + m_BoundingBox.Dimensions.x / 2.0f, position.y + m_BoundingBox.Dimensions.y / 2.0f, position.z + m_BoundingBox.Dimensions.z / 2.0f };
			m_CollisionMesh.SphereData.Center = m_Parent->GetPosition();
			break;
		}
		case CollisionMeshType::Plane:
		{
			std::vector<DirectX::XMVECTOR> transformedVertices;
			DirectX::XMMATRIX modelMatrix = m_Parent->GetModelMatrix();
			for (auto& vertex : m_BoundingBox.Vertices)
				transformedVertices.push_back(DirectX::XMVector3Transform(vertex, modelMatrix));
			std::vector<DirectX::XMVECTOR> updatedVertices = GetBoundingBoxVertices(transformedVertices);
			m_BoundingBox.MinPoint = { DirectX::XMVectorGetX(updatedVertices[0]), DirectX::XMVectorGetY(updatedVertices[0]), DirectX::XMVectorGetZ(updatedVertices[0]) };
			m_BoundingBox.MaxPoint = { DirectX::XMVectorGetX(updatedVertices[6]), DirectX::XMVectorGetY(updatedVertices[6]), DirectX::XMVectorGetZ(updatedVertices[6]) };
			m_CollisionMesh.PlaneData.Center = { DirectX::XMVectorGetX(m_Parent->GetPosition()), m_BoundingBox.MaxPoint.y, DirectX::XMVectorGetZ(m_Parent->GetPosition()) };
			//m_CollisionMesh.PlaneData.Normal = DirectX::XMVector3Normalize(m_Parent->GetRotation());
			m_CollisionMesh.PlaneData.Normal = { 0.0f, 1.0f, 0.0f, 0.0f };
			break;
		}
		}
	}

	void PhysicsComponent::ApplyForce(Force force)
	{
		if (!m_IsStatic)
			m_Forces.push_back(force);
	}

	void PhysicsComponent::SetMass(float mass)
	{
		m_Mass = mass;
		m_InvMass = (mass > 0.0f) ? 1.0f / mass : 0.0f;
		if (m_Mass <= 0.0f)
		{
			SetIsStatic(true);
		}
		else
		{
			SetIsStatic(false);
			UpdateInertiaTensor();
		}
	}

	void PhysicsComponent::SetIsStatic(bool isStatic)
	{
		m_IsStatic = isStatic;
		if (isStatic)
		{
			m_InverseInertiaTensor = DirectX::XMMatrixIdentity();
			m_InvMass = 0.0f;
		}
		UpdateCollisionMesh();
	}

	void PhysicsComponent::SetCollisionMeshType(CollisionMeshType type)
	{
		m_CollisionMesh.Type = type;
		UpdateCollisionMesh();
	}

	DirectX::XMVECTOR PhysicsComponent::GetPosition()
	{
		return m_Parent->GetPosition();
	}

	void PhysicsComponent::EvaluateForces(float ts)
	{
		for (Force& force : m_Forces)
		{
			if (force.Duration > 0.0f)
			{
				m_Acceleration = DirectX::XMVectorAdd(m_Acceleration, DirectX::XMVectorScale(force.Magnitude, 1.0f / m_Mass));
				force.Duration -= ts;
				if (!DirectX::XMVector3Equal(force.Point, DirectX::XMVectorZero()))
				{
					DirectX::XMVECTOR position = m_Parent->GetPosition();
					DirectX::XMVECTOR torque = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(force.Point, position), force.Magnitude);
					m_Torque = DirectX::XMVectorAdd(m_Torque, torque);
				}
			}
		}
		m_Forces.erase(std::remove_if(m_Forces.begin(), m_Forces.end(), [](Force f) { return f.Duration <= 0.0f; }), m_Forces.end());
	}

	void PhysicsComponent::UpdateInertiaTensor()
	{
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation());
		DirectX::XMMATRIX worldInertiaTensor = DirectX::XMMatrixMultiply(DirectX::XMMatrixTranspose(rotationMatrix), DirectX::XMMatrixMultiply(m_LocalInertiaTensor, rotationMatrix));
		m_InverseInertiaTensor = DirectX::XMMatrixInverse(nullptr, worldInertiaTensor);
	}

	std::vector<DirectX::XMVECTOR> PhysicsComponent::GetBoundingBoxVertices(std::vector<DirectX::XMVECTOR> transformedVertices)
	{
		float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
		for (const auto& vertex : transformedVertices)
		{
			float x = DirectX::XMVectorGetX(vertex);
			float y = DirectX::XMVectorGetY(vertex);
			float z = DirectX::XMVectorGetZ(vertex);
			minX = std::min(minX, x);
			minY = std::min(minY, y);
			minZ = std::min(minZ, z);
			maxX = std::max(maxX, x);
			maxY = std::max(maxY, y);
			maxZ = std::max(maxZ, z);
		}
		return {
			{ minX, minY, minZ }, { maxX, minY, minZ }, { maxX, maxY, minZ }, { minX, maxY, minZ },
			{ minX, minY, maxZ }, { maxX, minY, maxZ }, { maxX, maxY, maxZ }, { minX, maxY, maxZ }
		};
	}

	void PhysicsComponent::UpdateCollisionMesh()
	{
		OnTransformChanged(TransformType::Position);
		OnTransformChanged(TransformType::Rotation);
	}
}
