#pragma once
#include "Component.h"
#include "../Rendering/Buffers/VertexBuffer.h"
#include "../Rendering/Buffers/IndexBuffer.h"
#include "../Rendering/Buffers/ConstantBuffer.h"
#include "../Resources/Materials/Material.h"
#include "../Asset/ModelInstance.h"
#include <vector>

namespace DX12Engine
{
	class MeshPrimitive;

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

	struct ResolvedPrimitiveBinding
	{
		MeshPrimitive* Primitive = nullptr;
		Material* Material = nullptr;
	};

	class RenderComponent : public Component
	{
	public:
		friend class Renderer;
		friend class ProceduralRenderer;

		RenderComponent(GameObject* parent);
		RenderComponent(GameObject* parent, std::shared_ptr<ModelInstance> asset);
		~RenderComponent();

		virtual void Init() override;
		virtual void Update(float ts, float elapsed) override;

		virtual void OnTransformChanged(TransformType type) override;
	
		D3D12_GPU_VIRTUAL_ADDRESS GetCBVAddress() { return m_ConstantBuffer->GetGPUAddress(); }

		void SetAsset(std::shared_ptr<ModelInstance> asset);
		ModelInstance* GetAsset() const { return m_Asset.get(); }
		const std::vector<ResolvedPrimitiveBinding>& GetResolvedPrimitiveBindings() const { return m_ResolvedPrimitiveBindings; }

		DirectX::XMMATRIX GetModelMatrix();

	private:
		void UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition);
		void RebuildResolvedPrimitiveBindings();

		std::shared_ptr<ModelInstance> m_Asset;
		std::unique_ptr<ConstantBuffer> m_ConstantBuffer;
		RenderComponentData m_RenderObjectData;
		std::vector<ResolvedPrimitiveBinding> m_ResolvedPrimitiveBindings;
	};
}
