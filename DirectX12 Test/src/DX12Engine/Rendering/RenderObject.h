#pragma once
#include "RenderContext.h"
#include "../Resources/Mesh.h"
#include "../Buffers/VertexBuffer.h"
#include "../Buffers/IndexBuffer.h"
#include "../Buffers/ConstantBuffer.h"
#include "../Resources/Material.h"

namespace DX12Engine
{
	struct RenderObjectData
	{
		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMMATRIX WVPMatrix;

		void Reset()
		{
			ModelMatrix = DirectX::XMMatrixIdentity();
			WVPMatrix = DirectX::XMMatrixIdentity();
		}
	};

	class RenderObject
	{
	public:
		friend class Renderer;

		RenderObject(Mesh mesh);
		~RenderObject();

		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	
		void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX wvpMatrix);

		Mesh m_Mesh;
		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		RenderObjectData m_RenderObjectData;
		DirectX::XMMATRIX m_ModelMatrix;
		std::shared_ptr<Material> m_Material;
	};
}