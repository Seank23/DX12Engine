#include "RenderObject.h"

namespace DX12Engine
{
	RenderObject::RenderObject(std::shared_ptr<RenderContext> context, Mesh mesh)
		: m_RenderContext(context), m_Mesh(mesh), m_ModelMatrix(DirectX::XMMatrixIdentity()), m_ConstantBufferRes(nullptr)
	{
		m_VertexBuffer.SetData(context->GetDevice(), mesh.Vertices);
		m_IndexBuffer.SetData(context->GetDevice(), mesh.Indices);
		context->CreateConstantBuffer(m_ConstantBufferRes);
	}

	RenderObject::~RenderObject()
	{
		m_ConstantBufferRes.Reset();
		m_ConstantBufferData.Reset();
		m_Mesh.Reset();
		m_ModelMatrix = DirectX::XMMatrixIdentity();
	}

	void RenderObject::UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix)
	{
		m_ConstantBufferData.WVPMatrix = wvpMatrix;
	}
}
