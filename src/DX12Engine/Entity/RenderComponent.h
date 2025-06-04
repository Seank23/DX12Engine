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

		virtual void OnMeshChanged(Mesh* newMesh) override;
		virtual void OnTransformChanged(TransformType type) override;

		void SetMaterial(std::shared_ptr<Material> material) { m_Material = material; }

		Material* GetMaterial() { return m_Material.get(); }	
		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() { return m_VertexBuffer->GetVertexBufferView(); }
		D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() { return m_IndexBuffer->GetIndexBufferView(); }

		DirectX::XMMATRIX GetModelMatrix();

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition);

		std::unique_ptr<VertexBuffer> m_VertexBuffer;
		std::unique_ptr<IndexBuffer> m_IndexBuffer;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		RenderComponentData m_RenderObjectData;
		std::shared_ptr<Material> m_Material;
	};
}