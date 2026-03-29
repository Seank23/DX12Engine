#include "RenderComponent.h"
#include "../Resources/ResourceManager.h"
#include "GameObject.h"
#include "../Asset/MaterialAsset.h"
#include "../Asset/MeshAsset.h"
#include "../Asset/ModelAsset.h"

namespace DX12Engine
{
	RenderComponent::RenderComponent(GameObject* parent)
		: Component(parent, ComponentType::Render)
	{
	}

	RenderComponent::RenderComponent(GameObject* parent, std::shared_ptr<ModelInstance> asset)
		: Component(parent, ComponentType::Render), m_Asset(std::move(asset))
	{
		RebuildResolvedPrimitiveBindings();
	}

	RenderComponent::~RenderComponent()
	{
	}

	void RenderComponent::Init()
	{
		for (ResolvedPrimitiveBinding& binding : m_ResolvedPrimitiveBindings)
		{
			binding.PrimitiveConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderComponentData));
			binding.CBVAddress = binding.PrimitiveConstantBuffer->GetGPUAddress();
		}
	}

	void RenderComponent::Update(float ts, float elapsed)
	{
	}

	void RenderComponent::OnTransformChanged(TransformType type)
	{
	}

	void RenderComponent::SetAsset(std::shared_ptr<ModelInstance> asset)
	{
		m_Asset = std::move(asset);
		RebuildResolvedPrimitiveBindings();
	}

	DirectX::XMMATRIX RenderComponent::GetModelMatrix()
	{
		return m_Parent->GetModelMatrix();
	}

	void RenderComponent::RebuildResolvedPrimitiveBindings()
	{
		m_ResolvedPrimitiveBindings.clear();

		if (!m_Asset)
			return;

		ModelAsset* modelAsset = m_Asset->GetModelAsset();
		if (!modelAsset)
			return;

		// Build accumulated world transforms for every node by walking the hierarchy.
		const std::size_t nodeCount = modelAsset->GetNodeCount();
		std::vector<DirectX::XMFLOAT4X4> nodeWorldTransforms(nodeCount);
		for (std::size_t ni = 0; ni < nodeCount; ++ni)
		{
			const ModelNode* node = modelAsset->GetNode(ni);
			if (!node)
			{
				DirectX::XMStoreFloat4x4(&nodeWorldTransforms[ni], DirectX::XMMatrixIdentity());
				continue;
			}

			DirectX::XMMATRIX local = DirectX::XMLoadFloat4x4(&node->LocalTransform);
			if (node->ParentIndex >= 0 && static_cast<std::size_t>(node->ParentIndex) < ni)
			{
				DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&nodeWorldTransforms[node->ParentIndex]);
				DirectX::XMStoreFloat4x4(&nodeWorldTransforms[ni], parentWorld * local);
			}
			else
			{
				DirectX::XMStoreFloat4x4(&nodeWorldTransforms[ni], local);
			}
		}

		// For each node that references a mesh, emit one binding per primitive.
		for (std::size_t ni = 0; ni < nodeCount; ++ni)
		{
			const ModelNode* node = modelAsset->GetNode(ni);
			if (!node || node->MeshIndex < 0)
				continue;

			MeshAsset* meshAsset = modelAsset->GetMesh(static_cast<std::size_t>(node->MeshIndex));
			if (!meshAsset)
				continue;

			for (MeshPrimitive& primitive : meshAsset->GetPrimitives())
			{
				if (!primitive.HasGeometry())
					continue;

				MaterialAsset* materialAsset = m_Asset->ResolveMaterial(static_cast<std::size_t>(primitive.GetMaterialIndex()));
				Material* material = materialAsset ? materialAsset->GetMaterial() : nullptr;
				if (!material)
					material = ResourceManager::GetInstance().GetDefaultMaterial();

			ResolvedPrimitiveBinding binding;
			binding.Primitive = &primitive;
				binding.Material = material;
				binding.NodeWorldTransform = nodeWorldTransforms[ni];
				m_ResolvedPrimitiveBindings.push_back(std::move(binding));
			}
		}

		// Fallback: if no nodes reference any mesh (e.g. plain OBJ models that have no
		// node hierarchy), emit bindings directly from the flat mesh list.
		if (m_ResolvedPrimitiveBindings.empty())
		{
			DirectX::XMFLOAT4X4 identity;
			DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

			for (const std::shared_ptr<MeshAsset>& meshAsset : modelAsset->GetMeshes())
			{
				if (!meshAsset)
					continue;

				for (MeshPrimitive& primitive : meshAsset->GetPrimitives())
				{
					if (!primitive.HasGeometry())
						continue;

					MaterialAsset* materialAsset = m_Asset->ResolveMaterial(static_cast<std::size_t>(primitive.GetMaterialIndex()));
					Material* material = materialAsset ? materialAsset->GetMaterial() : nullptr;
					if (!material)
						material = ResourceManager::GetInstance().GetDefaultMaterial();

					ResolvedPrimitiveBinding binding;
					binding.Primitive = &primitive;
					binding.Material = material;
					binding.NodeWorldTransform = identity;
					m_ResolvedPrimitiveBindings.push_back(std::move(binding));
				}
			}
		}
	}

	void RenderComponent::UpdateConstantBufferData(ConstantBuffer& target, DirectX::XMMATRIX modelMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		m_RenderObjectData.ModelMatrix = modelMatrix;
		m_RenderObjectData.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = modelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		m_RenderObjectData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, projectionMatrix);
		m_RenderObjectData.CameraPosition = cameraPosition;
		target.Update(&m_RenderObjectData, sizeof(RenderComponentData));
	}
}
