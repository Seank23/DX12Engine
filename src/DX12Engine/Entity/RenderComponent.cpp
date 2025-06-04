#include "RenderComponent.h"
#include "../Resources/ResourceManager.h"
#include "GameObject.h"

namespace DX12Engine
{
	RenderComponent::RenderComponent(GameObject* parent)
		: Component(parent, ComponentType::Render)
	{
		OnMeshChanged(parent->GetMesh());
	}

	RenderComponent::~RenderComponent()
	{
	}

	void RenderComponent::Init()
	{
	}

	void RenderComponent::Update(float ts, float elapsed)
	{
	}

	void RenderComponent::OnMeshChanged(Mesh* newMesh)
	{
		if (newMesh)
		{
			m_VertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(newMesh->Vertices);
			m_IndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(newMesh->Indices);
			m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderComponentData));
		}
	}

	void RenderComponent::OnTransformChanged(TransformType type)
	{
	}

	DirectX::XMMATRIX RenderComponent::GetModelMatrix()
	{
		return m_Parent->GetModelMatrix();
	}

	void RenderComponent::UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		DirectX::XMMATRIX modelMatrix = m_Parent->GetModelMatrix();
		m_RenderObjectData.ModelMatrix = modelMatrix;
		m_RenderObjectData.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = modelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		m_RenderObjectData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, projectionMatrix);
		m_RenderObjectData.CameraPosition = cameraPosition;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderComponentData));
	}
}
