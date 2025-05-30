#pragma once
#include "Component.h"
#include "../Resources/Mesh.h"
#include "../Rendering/Buffers/VertexBuffer.h"
#include "../Rendering/Buffers/IndexBuffer.h"
#include "../Rendering/Buffers/ConstantBuffer.h"
#include "../Resources/Materials/Material.h"

namespace DX12Engine
{
	struct RenderComponentData
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

	class GameObject;

	class RenderComponent : public Component
	{
	public:
		friend class Renderer;
		friend class ProceduralRenderer;

		RenderComponent(GameObject* parent);
		~RenderComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;

		void SetMesh(Mesh mesh);
		void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

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
		RenderComponentData m_RenderObjectData;
		DirectX::XMMATRIX m_ModelMatrix;
		std::shared_ptr<Material> m_Material;
	};
}