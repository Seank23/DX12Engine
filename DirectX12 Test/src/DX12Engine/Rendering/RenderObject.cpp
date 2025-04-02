#include "RenderObject.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	RenderObject::RenderObject(Mesh mesh)
		: m_ModelMatrix(DirectX::XMMatrixIdentity()), m_Position({ 0.0f, 0.0f, 0.0f }), m_Scale({ 1.0f, 1.0f, 1.0f }), m_Rotation(DirectX::XMQuaternionIdentity())
	{
		SetMesh(mesh);
	}

	RenderObject::~RenderObject()
	{
		m_Mesh.Reset();
		m_ModelMatrix = DirectX::XMMatrixIdentity();
	}

	void RenderObject::SetMesh(Mesh mesh)
	{
		m_Mesh = mesh;
		m_VertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(mesh.Vertices);
		m_IndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(mesh.Indices);
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderObjectData));
	}

	void RenderObject::Move(DirectX::XMFLOAT3 movement)
	{
		m_Position = DirectX::XMVectorAdd(m_Position, DirectX::XMLoadFloat3(&movement));
		UpdateModelMatrix();
	}

	void RenderObject::Scale(DirectX::XMFLOAT3 scale)
	{
		m_Scale = DirectX::XMLoadFloat3(&scale);
		UpdateModelMatrix();
	}

	void RenderObject::Rotate(DirectX::XMFLOAT3 rotation)
	{
		DirectX::XMFLOAT3 toRadians({ DirectX::XMConvertToRadians(rotation.x), DirectX::XMConvertToRadians(rotation.y), DirectX::XMConvertToRadians(rotation.z) });
		DirectX::XMVECTOR quaternion = DirectX::XMQuaternionRotationRollPitchYawFromVector(DirectX::XMLoadFloat3(&toRadians));
		m_Rotation = DirectX::XMQuaternionMultiply(m_Rotation, quaternion);
		UpdateModelMatrix();
	}

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		m_RenderObjectData.ModelMatrix = m_ModelMatrix;
		m_RenderObjectData.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m_ModelMatrix));
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = m_ModelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.CameraPosition = cameraPosition;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderObjectData));
	}

	void RenderObject::UpdateModelMatrix()
	{
		m_ModelMatrix = DirectX::XMMatrixRotationQuaternion(m_Rotation) * DirectX::XMMatrixTranslationFromVector(m_Position) * DirectX::XMMatrixScalingFromVector(m_Scale);
	}
}
