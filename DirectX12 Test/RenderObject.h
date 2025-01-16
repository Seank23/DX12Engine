#pragma once
#include "RenderContext.h"
#include "Mesh.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"
#include "ConstantBufferData.h"

namespace DX12Engine
{
	class RenderObject
	{
	public:
		friend class Renderer;

		RenderObject(Mesh mesh);
		~RenderObject();

		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix);

		Mesh m_Mesh;
		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		ConstantBufferData m_ConstantBufferData;
		DirectX::XMMATRIX m_ModelMatrix;
	};
}