#include "RenderObject.h"
#include "ResourceManager.h"
#include "ConstantBufferData.h"

namespace DX12Engine
{
	RenderObject::RenderObject(Mesh mesh)
		: m_Mesh(mesh), m_ModelMatrix(DirectX::XMMatrixIdentity()), m_ConstantBufferData()
	{
		m_VertexBuffer = ResourceManager::GetInstance().CreateVertexBuffer(mesh.Vertices);
		m_IndexBuffer = ResourceManager::GetInstance().CreateIndexBuffer(mesh.Indices);
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(m_ConstantBufferData));
	}

	RenderObject::~RenderObject()
	{
		m_ConstantBufferData.Reset();
		m_Mesh.Reset();
		m_ModelMatrix = DirectX::XMMatrixIdentity();
	}

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix)
	{
		m_ConstantBufferData.WVPMatrix = wvpMatrix;
		m_ConstantBuffer->Update(&m_ConstantBufferData, sizeof(m_ConstantBufferData));
	}
}
