#pragma once
#include "../Resources/Mesh.h"
#include "../Buffers/VertexBuffer.h"
#include "../Buffers/IndexBuffer.h"
#include "../Buffers/ConstantBuffer.h"
#include "../Resources/Materials/Material.h"

namespace DX12Engine
{
	struct RenderObjectData
	{
		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMMATRIX NormalMatrix;
		DirectX::XMMATRIX ViewMatrix;
		DirectX::XMMATRIX ProjectionMatrix;
		DirectX::XMMATRIX MVPMatrix;
		DirectX::XMMATRIX InvViewMatrix;
		DirectX::XMMATRIX InvProjectionMatrix;
		DirectX::XMFLOAT3 CameraPosition;
		float Padding;
	};

	class RenderObject
	{
	public:
		friend class Renderer;
		friend class ProceduralRenderer;

		RenderObject() = default;
		RenderObject(Mesh mesh);
		~RenderObject();

		void SetMesh(Mesh mesh);
		void SetModelMatrix(DirectX::XMMATRIX modelMatrix) { m_ModelMatrix = modelMatrix; }	
		void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

		void Move(DirectX::XMFLOAT3 movement);
		void Scale(DirectX::XMFLOAT3 newScale);
		void Rotate(DirectX::XMFLOAT3 rotation);

		Material* GetMaterial() { return m_Material.get(); }	
		DirectX::XMMATRIX GetModelMatrix() { return m_ModelMatrix; }
		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() { return m_VertexBuffer->GetVertexBufferView(); }
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() { return m_IndexBuffer->GetIndexBufferView(); }

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition);
		void UpdateModelMatrix();

		Mesh m_Mesh;
		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		RenderObjectData m_RenderObjectData;
		DirectX::XMMATRIX m_ModelMatrix;
		std::shared_ptr<Material> m_Material;

		DirectX::XMVECTOR m_Position;
		DirectX::XMVECTOR m_Scale;
		DirectX::XMVECTOR m_Rotation;
	};
}