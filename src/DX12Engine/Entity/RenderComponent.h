#pragma once
#include "Component.h"
#include "../Rendering/Buffers/VertexBuffer.h"
#include "../Rendering/Buffers/IndexBuffer.h"
#include "../Rendering/Buffers/ConstantBuffer.h"
#include "../Resources/Materials/Material.h"
#include "../Asset/ModelInstance.h"
#include <memory>
#include <vector>

namespace DX12Engine
{
	class MeshPrimitive;
	class MaterialAsset;

	struct RenderComponentData
	{
		DirectX::XMMATRIX ModelMatrix;
		DirectX::XMMATRIX NormalMatrix;
		DirectX::XMMATRIX ViewMatrix;
		DirectX::XMMATRIX ProjectionMatrix;
		DirectX::XMMATRIX MVPMatrix;
		DirectX::XMFLOAT3 CameraPosition;
		float Padding;
		DirectX::XMMATRIX PrevMVPMatrix;
	};

	class GameObject;

	struct ResolvedPrimitiveBinding
	{
		MeshPrimitive* Primitive = nullptr;
		MaterialAsset* MaterialAsset = nullptr;
		DirectX::XMFLOAT4X4 NodeWorldTransform = [](){
			DirectX::XMFLOAT4X4 m{};
			DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixIdentity());
			return m;
		}();
		std::unique_ptr<ConstantBuffer> PrimitiveConstantBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS CBVAddress = 0;
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

		void SetAsset(std::shared_ptr<ModelInstance> asset);
		ModelInstance* GetAsset() const { return m_Asset.get(); }
		const std::vector<ResolvedPrimitiveBinding>& GetResolvedPrimitiveBindings() const { return m_ResolvedPrimitiveBindings; }
		std::vector<ResolvedPrimitiveBinding>& GetResolvedPrimitiveBindings() { return m_ResolvedPrimitiveBindings; }

		DirectX::XMMATRIX GetModelMatrix();

	private:
		void UpdateConstantBufferData(ConstantBuffer& target, DirectX::XMMATRIX modelMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition);
		void RebuildResolvedPrimitiveBindings();

		std::shared_ptr<ModelInstance> m_Asset;
		RenderComponentData m_RenderObjectData;
		bool m_HasValidPrevMVP = false;
		std::vector<ResolvedPrimitiveBinding> m_ResolvedPrimitiveBindings;
	};
}
