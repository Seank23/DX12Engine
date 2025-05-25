#include "RenderComponent.h"
#include "../Resources/ResourceManager.h"
#include "GameObject.h"

namespace DX12Engine
{
	RenderComponent::RenderComponent(GameObject* parent)
		: Component(parent, ComponentType::Render),
		m_ModelMatrix(DirectX::XMMatrixIdentity())
	{
	}

	RenderComponent::~RenderComponent()
	{
		m_Mesh.Reset();
		m_ModelMatrix = DirectX::XMMatrixIdentity();
	}

	void RenderComponent::Init()
	{
	}

	void RenderComponent::Update(float ts, float elapsed)
	{
		UpdateModelMatrix();
	}

	void RenderComponent::SetMesh(Mesh mesh)
	{
		m_Mesh = mesh;
		m_VertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(mesh.Vertices);
		m_IndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(mesh.Indices);
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderComponentData));
	}

	void RenderComponent::UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		m_RenderObjectData.ModelMatrix = m_ModelMatrix;
		m_RenderObjectData.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, m_ModelMatrix));
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = m_ModelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		m_RenderObjectData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, projectionMatrix);
		m_RenderObjectData.CameraPosition = cameraPosition;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderComponentData));
	}

	void RenderComponent::UpdateModelMatrix()
	{
		m_ModelMatrix = DirectX::XMMatrixRotationQuaternion(m_Parent->GetRotation()) * DirectX::XMMatrixTranslationFromVector(m_Parent->GetPosition()) * DirectX::XMMatrixScalingFromVector(m_Parent->GetScale());
	}
}
