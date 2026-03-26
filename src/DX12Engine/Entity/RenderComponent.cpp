#include "RenderComponent.h"
#include "../Resources/ResourceManager.h"
#include "GameObject.h"
#include "../Asset/MaterialAsset.h"
#include "../Asset/MeshAsset.h"

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
		m_ConstantBuffer = ResourceManager::GetInstance().CreateConstantBuffer(sizeof(RenderComponentData));
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

				m_ResolvedPrimitiveBindings.push_back({ &primitive, material });
			}
		}
	}

	void RenderComponent::UpdateConstantBufferData(DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix, DirectX::XMFLOAT3 cameraPosition)
	{
		DirectX::XMMATRIX modelMatrix = m_Parent->GetModelMatrix();
		m_RenderObjectData.ModelMatrix = modelMatrix;
		m_RenderObjectData.NormalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, modelMatrix));
		m_RenderObjectData.ViewMatrix = viewMatrix;
		m_RenderObjectData.ProjectionMatrix = projectionMatrix;
		m_RenderObjectData.MVPMatrix = modelMatrix * viewMatrix * projectionMatrix;
		m_RenderObjectData.InvViewMatrix = DirectX::XMMatrixInverse(nullptr, viewMatrix);
		m_RenderObjectData.InvProjectionMatrix = DirectX::XMMatrixInverse(nullptr, projectionMatrix);
		m_RenderObjectData.CameraPosition = cameraPosition;
		m_ConstantBuffer->Update(&m_RenderObjectData, sizeof(RenderComponentData));
	}
}
