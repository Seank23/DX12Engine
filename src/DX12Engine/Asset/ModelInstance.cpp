#include "ModelInstance.h"
#include "../Utils/AnimationUtils.h"

#include <algorithm>
#include <cmath>

namespace DX12Engine
{
	ModelInstance::ModelInstance()
	{
		DirectX::XMStoreFloat4x4(&m_WorldTransform, DirectX::XMMatrixIdentity());
	}

	ModelInstance::ModelInstance(std::shared_ptr<ModelAsset> modelAsset)
		: ModelInstance()
	{
		SetModelAsset(std::move(modelAsset));
	}

	void ModelInstance::SetModelAsset(std::shared_ptr<ModelAsset> modelAsset)
	{
		m_ModelAsset = std::move(modelAsset);
		ResetNodeTransforms();
		if (!m_ModelAsset)
		{
			m_MaterialOverrides.clear();
			return;
		}

		const auto& nodes = m_ModelAsset->GetNodes();
		if (m_LocalNodeTransforms.size() != nodes.size())
		{
			m_LocalNodeTransforms.clear();
			m_LocalNodeTransforms.reserve(nodes.size());
			for (const ModelNode& node : nodes)
				m_LocalNodeTransforms.push_back(node.LocalTransform);
		}

		const std::size_t materialCount = m_ModelAsset->GetMaterialCount();
		if (m_MaterialOverrides.size() < materialCount)
			m_MaterialOverrides.resize(materialCount);
	}

	void ModelInstance::ResetNodeTransforms()
	{
		m_WorldNodeTransforms.clear();
		m_LocalNodeTransforms.clear();
		m_LocalNodeTransforms.shrink_to_fit();
		const auto& nodes = m_ModelAsset->GetNodes();
		m_WorldNodeTransforms.resize(nodes.size());
	}

	void ModelInstance::UpdateNodeWorldTransforms()
	{
		if (!m_ModelAsset || m_LocalNodeTransforms.empty() || !m_NodeTransformsDirty)
			return;

		const auto& nodes = m_ModelAsset->GetNodes();

		for (std::size_t i = 0; i < nodes.size(); ++i)
		{
			DirectX::XMMATRIX local = DirectX::XMLoadFloat4x4(&m_LocalNodeTransforms[i]);

			if (nodes[i].ParentIndex >= 0 && static_cast<std::size_t>(nodes[i].ParentIndex) < i)
			{
				DirectX::XMMATRIX parentWorld = DirectX::XMLoadFloat4x4(&m_WorldNodeTransforms[nodes[i].ParentIndex]);
				DirectX::XMStoreFloat4x4(&m_WorldNodeTransforms[i], parentWorld * local);
			}
			else
			{
				DirectX::XMStoreFloat4x4(&m_WorldNodeTransforms[i], local);
			}
		}
		m_NodeTransformsDirty = false;
	}

	DirectX::XMMATRIX ModelInstance::GetNodeWorldTransform(std::size_t nodeIndex)
	{
		UpdateNodeWorldTransforms();

		if (nodeIndex >= m_WorldNodeTransforms.size())
			return DirectX::XMMatrixIdentity();

		return DirectX::XMLoadFloat4x4(&m_WorldNodeTransforms[nodeIndex]);
	}

	void ModelInstance::SetMaterialOverride(std::size_t materialIndex, std::shared_ptr<MaterialAsset> materialAsset)
	{
		if (m_MaterialOverrides.size() <= materialIndex)
			m_MaterialOverrides.resize(materialIndex + 1);

		m_MaterialOverrides[materialIndex] = std::move(materialAsset);
	}

	void ModelInstance::ClearMaterialOverride(std::size_t materialIndex)
	{
		if (materialIndex < m_MaterialOverrides.size())
			m_MaterialOverrides[materialIndex].reset();
	}

	void ModelInstance::ClearMaterialOverrides()
	{
		for (auto& overrideMaterial : m_MaterialOverrides)
			overrideMaterial.reset();
	}

	MaterialAsset* ModelInstance::ResolveMaterial(std::size_t materialIndex) const
	{
		if (materialIndex < m_MaterialOverrides.size() && m_MaterialOverrides[materialIndex] != nullptr)
			return m_MaterialOverrides[materialIndex].get();

		if (!m_ModelAsset)
			return nullptr;

		return m_ModelAsset->GetMaterial(materialIndex);
	}
}
