#pragma once
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

		RenderObject(Microsoft::WRL::ComPtr<ID3D12Device> device, Mesh mesh);
		~RenderObject();

		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix);

		Mesh m_Mesh;
		VertexBuffer m_VertexBuffer;
		IndexBuffer m_IndexBuffer;
		ConstantBuffer m_ConstantBufferData;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_ConstantBufferRes;
		DirectX::XMMATRIX m_ModelMatrix;
	};
}