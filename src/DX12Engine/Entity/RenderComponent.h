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
		DirectX::XMMATRIX UnjitteredMVPMatrix;
	};

	class GameObject;

	struct ResolvedPrimitiveBinding
	{
		MeshPrimitive* Primitive = nullptr;
		MaterialAsset* MaterialAsset = nullptr;
		int NodeIndex = -1;
		std::unique_ptr<ConstantBuffer> PrimitiveConstantBuffer;
		D3D12_GPU_VIRTUAL_ADDRESS CBVAddress = 0;
		DirectX::XMMATRIX PrevUnjitteredMVPMatrix = DirectX::XMMatrixIdentity();
		bool HasValidPrevUnjitteredMVP = false;
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

		void PlayAnimation(const std::string& animationName, bool loop = true);

	private:
		void UpdateConstantBufferData(
			ResolvedPrimitiveBinding& binding,
			DirectX::XMMATRIX modelMatrix,
			DirectX::XMMATRIX viewMatrix,
			DirectX::XMMATRIX projectionMatrix,
			DirectX::XMMATRIX unjitteredProjectionMatrix,
			DirectX::XMFLOAT3 cameraPosition);
		void RebuildResolvedPrimitiveBindings();

		std::shared_ptr<ModelInstance> m_Asset;
		RenderComponentData m_RenderObjectData;
		std::vector<ResolvedPrimitiveBinding> m_ResolvedPrimitiveBindings;
	};
}
