#pragma once
#include "ModelAsset.h"
#include <DirectXMath.h>
#include <cstddef>
#include <memory>
#include <vector>

namespace DX12Engine
{
	class ModelInstance
	{
	public:
		ModelInstance()
		{
			DirectX::XMStoreFloat4x4(&m_WorldTransform, DirectX::XMMatrixIdentity());
		}

		explicit ModelInstance(std::shared_ptr<ModelAsset> modelAsset)
			: ModelInstance()
		{
			SetModelAsset(std::move(modelAsset));
		}

		void SetModelAsset(std::shared_ptr<ModelAsset> modelAsset)
		{
			m_ModelAsset = std::move(modelAsset);
			if (!m_ModelAsset)
			{
				m_MaterialOverrides.clear();
				return;
			}

			const std::size_t materialCount = m_ModelAsset->GetMaterialCount();
			if (m_MaterialOverrides.size() < materialCount)
				m_MaterialOverrides.resize(materialCount);
		}

		std::shared_ptr<ModelAsset> GetModelAssetShared() const { return m_ModelAsset; }
		ModelAsset* GetModelAsset() const { return m_ModelAsset.get(); }

		void SetWorldTransform(const DirectX::XMMATRIX& worldTransform)
		{
			DirectX::XMStoreFloat4x4(&m_WorldTransform, worldTransform);
		}

		DirectX::XMMATRIX GetWorldTransformMatrix() const
		{
			return DirectX::XMLoadFloat4x4(&m_WorldTransform);
		}

		const DirectX::XMFLOAT4X4& GetWorldTransform() const { return m_WorldTransform; }

		void SetMaterialOverride(std::size_t materialIndex, std::shared_ptr<MaterialAsset> materialAsset)
		{
			if (m_MaterialOverrides.size() <= materialIndex)
				m_MaterialOverrides.resize(materialIndex + 1);

			m_MaterialOverrides[materialIndex] = std::move(materialAsset);
		}

		void ClearMaterialOverride(std::size_t materialIndex)
		{
			if (materialIndex < m_MaterialOverrides.size())
				m_MaterialOverrides[materialIndex].reset();
		}

		void ClearMaterialOverrides()
		{
			for (auto& overrideMaterial : m_MaterialOverrides)
				overrideMaterial.reset();
		}

		MaterialAsset* ResolveMaterial(std::size_t materialIndex) const
		{
			if (materialIndex < m_MaterialOverrides.size() && m_MaterialOverrides[materialIndex] != nullptr)
				return m_MaterialOverrides[materialIndex].get();

			if (!m_ModelAsset)
				return nullptr;

			return m_ModelAsset->GetMaterial(materialIndex);
		}

		const std::vector<std::shared_ptr<MaterialAsset>>& GetMaterialOverrides() const { return m_MaterialOverrides; }

	private:
		std::shared_ptr<ModelAsset> m_ModelAsset;
		DirectX::XMFLOAT4X4 m_WorldTransform{};
		std::vector<std::shared_ptr<MaterialAsset>> m_MaterialOverrides;
	};
}
