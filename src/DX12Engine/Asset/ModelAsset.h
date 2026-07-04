#pragma once
#include "MaterialAsset.h"
#include "MeshAsset.h"
#include "Animation.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace DX12Engine
{
	struct ModelNode
	{
		std::string Name;
		int ParentIndex = -1;
		int MeshIndex = -1;
		DirectX::XMFLOAT4X4 LocalTransform = []()
		{
			DirectX::XMFLOAT4X4 mat{};
			DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
			return mat;
		}();
	};

	class ModelAsset
	{
	public:
		ModelAsset() = default;
		explicit ModelAsset(std::string name)
			: m_Name(std::move(name))
		{
		}

		void SetName(std::string name) { m_Name = std::move(name); }
		const std::string& GetName() const { return m_Name; }

		std::size_t AddMesh(std::shared_ptr<MeshAsset> meshAsset)
		{
			m_Meshes.emplace_back(std::move(meshAsset));
			return m_Meshes.size() - 1;
		}

		std::size_t AddMaterial(std::shared_ptr<MaterialAsset> materialAsset)
		{
			m_Materials.emplace_back(std::move(materialAsset));
			return m_Materials.size() - 1;
		}

		std::size_t AddNode(const ModelNode& node)
		{
			m_Nodes.emplace_back(node);
			return m_Nodes.size() - 1;
		}

		void ClearMeshes() { m_Meshes.clear(); }
		void ClearMaterials() { m_Materials.clear(); }
		void ClearNodes() { m_Nodes.clear(); }

		std::size_t GetMeshCount() const { return m_Meshes.size(); }
		std::size_t GetMaterialCount() const { return m_Materials.size(); }
		std::size_t GetNodeCount() const { return m_Nodes.size(); }

		MeshAsset* GetMesh(std::size_t index) const
		{
			if (index >= m_Meshes.size() || m_Meshes[index] == nullptr)
				return nullptr;
			return m_Meshes[index].get();
		}

		MaterialAsset* GetMaterial(std::size_t index) const
		{
			if (index >= m_Materials.size() || m_Materials[index] == nullptr)
				return nullptr;
			return m_Materials[index].get();
		}

		MaterialAsset* GetMaterial(std::string name) const
		{
			auto matIt = std::find_if(m_Materials.begin(), m_Materials.end(),
				[&name](const std::shared_ptr<MaterialAsset>& mat) { return mat && mat->GetName() == name; });
			if (matIt == m_Materials.end() || *matIt == nullptr)
				return nullptr;
			return (*matIt).get();
		}

		ModelNode* GetNode(std::size_t index)
		{
			if (index >= m_Nodes.size())
				return nullptr;
			return &m_Nodes[index];
		}

		const ModelNode* GetNode(std::size_t index) const
		{
			if (index >= m_Nodes.size())
				return nullptr;
			return &m_Nodes[index];
		}

		std::vector<std::shared_ptr<MeshAsset>>& GetMeshes() { return m_Meshes; }
		const std::vector<std::shared_ptr<MeshAsset>>& GetMeshes() const { return m_Meshes; }
		std::vector<std::shared_ptr<MaterialAsset>>& GetMaterials() { return m_Materials; }
		const std::vector<std::shared_ptr<MaterialAsset>>& GetMaterials() const { return m_Materials; }
		std::vector<ModelNode>& GetNodes() { return m_Nodes; }
		const std::vector<ModelNode>& GetNodes() const { return m_Nodes; }

		size_t AddAnimation(const AnimationClip& animation, const std::string& name = "")
		{
			std::string animName = name.empty() ? animation.Name : name;
			m_Animations[animName] = animation;
			return m_Animations.size() - 1;
		}
		const AnimationClip& GetAnimation(const std::string& name) const { return m_Animations.at(name); }
		size_t GetAnimationCount() const { return m_Animations.size(); }
		bool DoesAnimationExist(const std::string& name) const { return m_Animations.find(name) != m_Animations.end(); }

	private:
		std::string m_Name;
		std::vector<std::shared_ptr<MeshAsset>> m_Meshes;
		std::vector<std::shared_ptr<MaterialAsset>> m_Materials;
		std::vector<ModelNode> m_Nodes;
		std::unordered_map<std::string, AnimationClip> m_Animations;
	};
}
