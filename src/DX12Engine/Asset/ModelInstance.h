#pragma once
#include "ModelAsset.h"
#include "Animation.h"
#include <DirectXMath.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace DX12Engine
{
	class ModelInstance
	{
	public:
		ModelInstance();
		explicit ModelInstance(std::shared_ptr<ModelAsset> modelAsset);
		
		void SetModelAsset(std::shared_ptr<ModelAsset> modelAsset);
		std::shared_ptr<ModelAsset> GetModelAssetShared() const { return m_ModelAsset; }
		ModelAsset* GetModelAsset() const { return m_ModelAsset.get(); }

		void SetWorldTransform(const DirectX::XMMATRIX& worldTransform) { DirectX::XMStoreFloat4x4(&m_WorldTransform, worldTransform); }
		DirectX::XMMATRIX GetWorldTransformMatrix() const { return DirectX::XMLoadFloat4x4(&m_WorldTransform); }
		const DirectX::XMFLOAT4X4& GetWorldTransform() const { return m_WorldTransform; }

		void SetMaterialOverride(std::size_t materialIndex, std::shared_ptr<MaterialAsset> materialAsset);
		void ClearMaterialOverride(std::size_t materialIndex);
		void ClearMaterialOverrides();
		const std::vector<std::shared_ptr<MaterialAsset>>& GetMaterialOverrides() const { return m_MaterialOverrides; }
		MaterialAsset* ResolveMaterial(std::size_t materialIndex) const;

		void ResetNodeTransforms();
		void UpdateNodeWorldTransforms();
		DirectX::XMMATRIX GetNodeWorldTransform(std::size_t nodeIndex);

		void PlayAnimation(const std::string& animationName, bool loop = true, bool reverse = false);
		void UpdateAnimation(float deltaSeconds);

	private:
		struct ActiveAnimationState
		{
			const AnimationClip* Clip = nullptr;
			float Time = 0.0f;
			float Weight = 1.0f;
			bool Loop = true;
			bool Reverse = false;
		};

		std::shared_ptr<ModelAsset> m_ModelAsset;
		DirectX::XMFLOAT4X4 m_WorldTransform{};
		std::vector<std::shared_ptr<MaterialAsset>> m_MaterialOverrides;

		std::vector<DirectX::XMFLOAT4X4> m_LocalNodeTransforms;
		std::vector<DirectX::XMFLOAT4X4> m_WorldNodeTransforms;
		std::vector<DirectX::XMFLOAT4X4> m_BaseNodeTransforms;
		bool m_NodeTransformsDirty = true;

		std::vector<ActiveAnimationState> m_ActiveAnimations;
		float m_AnimationSpeed = 1.0f;
	};
}
