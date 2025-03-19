#include "RenderObject.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	RenderObject::RenderObject(Mesh mesh)
		: m_ModelMatrix(DirectX::XMMatrixIdentity())
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

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		m_RenderObjectData.ModelMatrix = m_ModelMatrix;
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = m_ModelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.CameraPosition = cameraPosition;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderObjectData));
	}
}
