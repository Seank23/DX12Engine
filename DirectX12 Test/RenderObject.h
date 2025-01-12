#pragma once
#include "RenderContext.h"
#include "Mesh.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "ConstantBuffer.h"

namespace DX12Engine
{
	class RenderObject
	{
	public:
		friend class Renderer;

		RenderObject(std::shared_ptr<RenderContext> context, Mesh mesh);
		~RenderObject();

		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix);

		std::shared_ptr<RenderContext> m_RenderContext;
		Mesh m_Mesh;
		VertexBuffer m_VertexBuffer;
		IndexBuffer m_IndexBuffer;
		ConstantBuffer m_ConstantBufferData;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBufferRes;
		DirectX::XMMATRIX m_ModelMatrix;
	};
}