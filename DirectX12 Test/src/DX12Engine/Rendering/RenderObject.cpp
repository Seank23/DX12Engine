#include "RenderObject.h"
#include "../Resources/ResourceManager.h"

namespace DX12Engine
{
	RenderObject::RenderObject(Mesh mesh)
		: m_Mesh(mesh), m_ModelMatrix(DirectX::XMMatrixIdentity())
	{
		m_VertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(mesh.Vertices);
		m_IndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(mesh.Indices);
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderObjectData));
	}

	RenderObject::~RenderObject()
	{
		m_RenderObjectData.Reset();
		m_Mesh.Reset();
		m_ModelMatrix = DirectX::XMMatrixIdentity();
	}

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix)
	{
		m_RenderObjectData.ModelMatrix = m_ModelMatrix;
		m_RenderObjectData.WVPMatrix = wvpMatrix;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderObjectData));
	}
}
